/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef _MSC_VER
#pragma comment(lib, "opengl32")
#endif

#include "../driver.h"
#include "../performance.h"
#include "scaler/scaler.h"
#include "image/image.h"
#include "../file.h"

#include <stdint.h>
#include "../libretro.h"
#include <stdio.h>
#include <string.h>
#include "../general.h"
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gl_common.h"
#include "gfx_common.h"
#include "gfx_context.h"
#include "../compat/strl.h"

#ifdef HAVE_CG
#include "shader_cg.h"
#endif

#ifdef HAVE_GLSL
#include "shader_glsl.h"
#endif

#include "shader_common.h"

// Used for the last pass when rendering to the back buffer.
static const GLfloat vertexes_flipped[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

// Used when rendering to an FBO.
// Texture coords have to be aligned with vertex coordinates.
static const GLfloat vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat tex_coords[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat white_color[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
};

static inline bool gl_query_extension(gl_t *gl, const char *ext)
{
   bool ret = false;

   if (gl->core_context)
   {
#ifdef GL_NUM_EXTENSIONS
      GLint i, exts;
      exts = 0;
      glGetIntegerv(GL_NUM_EXTENSIONS, &exts);
      for (i = 0; i < exts; i++)
      {
         const char *str = (const char*)glGetStringi(GL_EXTENSIONS, i);
         if (str && strstr(str, ext))
         {
            ret = true;
            break;
         }
      }
#endif
   }
   else
   {
      const char *str = (const char*)glGetString(GL_EXTENSIONS);
      ret = str && strstr(str, ext);
   }

   RARCH_LOG("Querying GL extension: %s => %s\n",
         ext, ret ? "exists" : "doesn't exist");
   return ret;
}

#ifdef HAVE_OVERLAY
static void gl_render_overlay(void *data);
static void gl_overlay_vertex_geom(void *data,
      unsigned image,
      float x, float y, float w, float h);
static void gl_overlay_tex_geom(void *data,
      unsigned image,
      float x, float y, float w, float h);
#endif

static inline void set_texture_coords(GLfloat *coords, GLfloat xamt, GLfloat yamt)
{
   coords[2] = xamt;
   coords[6] = xamt;
   coords[5] = yamt;
   coords[7] = yamt;
}

#if defined(HAVE_EGL) && defined(HAVE_OPENGLES2)
static bool check_eglimage_proc(void)
{
   return glEGLImageTargetTexture2DOES != NULL;
}
#endif

#ifdef HAVE_GL_SYNC
static bool check_sync_proc(gl_t *gl)
{
   if (!gl_query_extension(gl, "ARB_sync"))
      return false;

   return glFenceSync && glDeleteSync && glClientWaitSync;
}
#endif

#ifndef HAVE_OPENGLES
static bool init_vao(gl_t *gl)
{
   if (!gl->core_context && !gl_query_extension(gl, "ARB_vertex_array_object"))
      return false;

   bool present = glGenVertexArrays && glBindVertexArray && glDeleteVertexArrays;
   if (!present)
      return false;

   glGenVertexArrays(1, &gl->vao);
   return true;
}
#endif

#ifdef HAVE_FBO
#if defined(HAVE_PSGL)
#define glGenFramebuffers glGenFramebuffersOES
#define glBindFramebuffer glBindFramebufferOES
#define glFramebufferTexture2D glFramebufferTexture2DOES
#define glCheckFramebufferStatus glCheckFramebufferStatusOES
#define glDeleteFramebuffers glDeleteFramebuffersOES
#define glGenRenderbuffers glGenRenderbuffersOES
#define glBindRenderbuffer glBindRenderbufferOES
#define glFramebufferRenderbuffer glFramebufferRenderbufferOES
#define glRenderbufferStorage glRenderbufferStorageOES
#define glDeleteRenderbuffers glDeleteRenderbuffersOES
#define GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#define GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#define GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
#define check_fbo_proc(gl) (true)
#elif !defined(HAVE_OPENGLES2)
static bool check_fbo_proc(gl_t *gl)
{
   if (!gl->core_context && !gl_query_extension(gl, "ARB_framebuffer_object"))
      return false;

   return glGenFramebuffers && glBindFramebuffer && glFramebufferTexture2D && 
      glCheckFramebufferStatus && glDeleteFramebuffers &&
      glGenRenderbuffers && glBindRenderbuffer &&
      glFramebufferRenderbuffer && glRenderbufferStorage &&
      glDeleteRenderbuffers;
}
#else
#define check_fbo_proc(gl) (true)
#endif
#if defined(__APPLE__) || defined(HAVE_PSGL)
#define GL_RGBA32F GL_RGBA32F_ARB
#endif
#endif

////////////////// Shaders

static bool gl_shader_init(void *data)
{
   gl_t *gl = (gl_t*)data;
   const gl_shader_backend_t *backend = NULL;

   const char *shader_path = (g_settings.video.shader_enable && *g_settings.video.shader_path) ?
      g_settings.video.shader_path : NULL;

   enum rarch_shader_type type = gfx_shader_parse_type(shader_path,
         gl->core_context ? RARCH_SHADER_GLSL : DEFAULT_SHADER_TYPE);

   if (type == RARCH_SHADER_NONE)
   {
      RARCH_LOG("[GL]: Not loading any shader.\n");
      return true;
   }

   switch (type)
   {
#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         RARCH_LOG("[GL]: Using Cg shader backend.\n");
         backend = &gl_cg_backend;
         break;
#endif

#ifdef HAVE_GLSL
      case RARCH_SHADER_GLSL:
         RARCH_LOG("[GL]: Using GLSL shader backend.\n");
         backend = &gl_glsl_backend;
         break;
#endif

      default:
         break;
   }

   if (!backend)
   {
      RARCH_ERR("[GL]: Didn't find valid shader backend. Continuing without shaders.\n");
      return true;
   }

#ifdef HAVE_GLSL
   if (gl->core_context && RARCH_SHADER_CG)
   {
      RARCH_ERR("[GL]: Cg cannot be used with core GL context. Falling back to GLSL.\n");
      backend = &gl_glsl_backend;
      shader_path = NULL;
   }
#endif

   gl->shader = backend;
   bool ret = gl->shader->init(gl, shader_path);
   if (!ret)
   {
      RARCH_ERR("[GL]: Failed to init shader, falling back to stock.\n");
      ret = gl->shader->init(gl, NULL);
   }

   return ret;
}

static inline void gl_shader_deinit(void *data)
{
   gl_t *gl = (gl_t*)data;

   if (gl->shader)
      gl->shader->deinit();
   gl->shader = NULL;
}

#ifndef NO_GL_FF_VERTEX
static void gl_set_coords(const struct gl_coords *coords)
{
   glClientActiveTexture(GL_TEXTURE1);
   glTexCoordPointer(2, GL_FLOAT, 0, coords->lut_tex_coord);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

   glClientActiveTexture(GL_TEXTURE0);
   glVertexPointer(2, GL_FLOAT, 0, coords->vertex);
   glEnableClientState(GL_VERTEX_ARRAY);

   glColorPointer(4, GL_FLOAT, 0, coords->color);
   glEnableClientState(GL_COLOR_ARRAY);

   glTexCoordPointer(2, GL_FLOAT, 0, coords->tex_coord);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

static void gl_disable_client_arrays(gl_t *gl)
{
   if (gl->core_context)
      return;

   glClientActiveTexture(GL_TEXTURE1);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glClientActiveTexture(GL_TEXTURE0);
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_COLOR_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}
#endif

#ifndef NO_GL_FF_MATRIX
static void gl_set_mvp(const void *data)
{
   const math_matrix *mat = (const math_matrix*)data;
   glMatrixMode(GL_PROJECTION);
   glLoadMatrixf(mat->data);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
}
#endif

void gl_shader_set_coords(void *data, const struct gl_coords *coords, const math_matrix *mat)
{
   gl_t *gl = (gl_t*)data;

   bool ret_coords = false;
   bool ret_mvp    = false;

   (void)ret_coords;
   (void)ret_mvp;

   if (gl->shader)
      ret_coords = gl->shader->set_coords(coords);
   if (gl->shader)
      ret_mvp = gl->shader->set_mvp(gl, mat);

   // Fall back to FF-style if needed and possible.
#ifndef NO_GL_FF_VERTEX
   if (!ret_coords)
      gl_set_coords(coords);
#endif

#ifndef NO_GL_FF_MATRIX
   if (!ret_mvp)
      gl_set_mvp(mat);
#endif
}

#define gl_shader_num(gl) ((gl->shader) ? gl->shader->num_shaders() : 0)
#define gl_shader_filter_type(gl, index, smooth) ((gl->shader) ? gl->shader->filter_type(index, smooth) : false)
#define gl_shader_wrap_type(gl, index) ((gl->shader) ? gl->shader->wrap_type(index) : RARCH_WRAP_BORDER)

#ifdef IOS
// There is no default frame buffer on IOS.
void apple_bind_game_view_fbo(void);
#define gl_bind_backbuffer() apple_bind_game_view_fbo()
#else
#define gl_bind_backbuffer() glBindFramebuffer(GL_FRAMEBUFFER, 0)
#endif

#ifdef HAVE_FBO
static void gl_shader_scale(void *data, unsigned index, struct gfx_fbo_scale *scale)
{
   gl_t *gl = (gl_t*)data;

   scale->valid = false;
   if (gl->shader)
      gl->shader->shader_scale(index, scale);
}

static void gl_compute_fbo_geometry(void *data, unsigned width, unsigned height,
      unsigned vp_width, unsigned vp_height)
{
   int i;
   unsigned last_width, last_height, last_max_width, last_max_height;
   gl_t *gl = (gl_t*)data;
   last_width = width;
   last_height = height;
   last_max_width = gl->tex_w;
   last_max_height = gl->tex_h;

   GLint max_size = 0;
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
   bool size_modified = false;

   // Calculate viewports for FBOs.
   for (i = 0; i < gl->fbo_pass; i++)
   {
      switch (gl->fbo_scale[i].type_x)
      {
         case RARCH_SCALE_INPUT:
            gl->fbo_rect[i].img_width = last_width * gl->fbo_scale[i].scale_x;
            gl->fbo_rect[i].max_img_width = last_max_width * gl->fbo_scale[i].scale_x;
            break;

         case RARCH_SCALE_ABSOLUTE:
            gl->fbo_rect[i].img_width = gl->fbo_rect[i].max_img_width = gl->fbo_scale[i].abs_x;
            break;

         case RARCH_SCALE_VIEWPORT:
            gl->fbo_rect[i].img_width = gl->fbo_rect[i].max_img_width = gl->fbo_scale[i].scale_x * vp_width;
            break;

         default:
            break;
      }

      switch (gl->fbo_scale[i].type_y)
      {
         case RARCH_SCALE_INPUT:
            gl->fbo_rect[i].img_height = last_height * gl->fbo_scale[i].scale_y;
            gl->fbo_rect[i].max_img_height = last_max_height * gl->fbo_scale[i].scale_y;
            break;

         case RARCH_SCALE_ABSOLUTE:
            gl->fbo_rect[i].img_height = gl->fbo_rect[i].max_img_height = gl->fbo_scale[i].abs_y;
            break;

         case RARCH_SCALE_VIEWPORT:
            gl->fbo_rect[i].img_height = gl->fbo_rect[i].max_img_height = gl->fbo_scale[i].scale_y * vp_height;
            break;

         default:
            break;
      }

      if (gl->fbo_rect[i].img_width > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].img_width = max_size;
      }

      if (gl->fbo_rect[i].img_height > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].img_height = max_size;
      }

      if (gl->fbo_rect[i].max_img_width > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].max_img_width = max_size;
      }

      if (gl->fbo_rect[i].max_img_height > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].max_img_height = max_size;
      }

      if (size_modified)
         RARCH_WARN("FBO textures exceeded maximum size of GPU (%ux%u). Resizing to fit.\n", max_size, max_size);

      last_width = gl->fbo_rect[i].img_width;
      last_height = gl->fbo_rect[i].img_height;
      last_max_width = gl->fbo_rect[i].max_img_width;
      last_max_height = gl->fbo_rect[i].max_img_height;
   }
}

static void gl_create_fbo_textures(void *data)
{
   int i;
   gl_t *gl = (gl_t*)data;

   glGenTextures(gl->fbo_pass, gl->fbo_texture);

   GLuint base_filt = g_settings.video.smooth ? GL_LINEAR : GL_NEAREST;
   for (i = 0; i < gl->fbo_pass; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i]);

      GLuint filter_type = base_filt;
      bool smooth = false;
      if (gl_shader_filter_type(gl, i + 2, &smooth))
         filter_type = smooth ? GL_LINEAR : GL_NEAREST;

      enum gfx_wrap_type wrap = gl_shader_wrap_type(gl, i + 2);
      GLenum wrap_enum = gl_wrap_type_to_enum(wrap);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_enum);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_enum);

      bool fp_fbo = gl->fbo_scale[i].valid && gl->fbo_scale[i].fp_fbo;

      if (fp_fbo && !gl->has_fp_fbo)
         RARCH_ERR("Floating-point FBO was requested, but is not supported. Falling back to UNORM.\n");

      if (fp_fbo && gl->has_fp_fbo)
      {
         RARCH_LOG("FBO pass #%d is floating-point.\n", i);
         // GLES and GL are inconsistent in which arguments to pass.
#ifdef HAVE_OPENGLES2
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
               gl->fbo_rect[i].width, gl->fbo_rect[i].height,
               0, GL_RGBA, GL_FLOAT, NULL);
#else
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
               gl->fbo_rect[i].width, gl->fbo_rect[i].height,
               0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
#endif
      }
      else
      {
#ifdef HAVE_OPENGLES2
         glTexImage2D(GL_TEXTURE_2D,
               0, GL_RGBA,
               gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, NULL);
#else
         // Avoid potential performance reductions on particular platforms.
         glTexImage2D(GL_TEXTURE_2D,
               0, RARCH_GL_INTERNAL_FORMAT32,
               gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
               RARCH_GL_TEXTURE_TYPE32, RARCH_GL_FORMAT32, NULL);
#endif
      }
   }

   glBindTexture(GL_TEXTURE_2D, 0);
}

static bool gl_create_fbo_targets(void *data)
{
   int i;
   gl_t *gl = (gl_t*)data;

   glBindTexture(GL_TEXTURE_2D, 0);
   glGenFramebuffers(gl->fbo_pass, gl->fbo);
   for (i = 0; i < gl->fbo_pass; i++)
   {
      glBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[i]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

      GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE)
         goto error;
   }

   return true;

error:
   glDeleteFramebuffers(gl->fbo_pass, gl->fbo);
   RARCH_ERR("Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
   return false;
}

void gl_deinit_fbo(void *data)
{
   gl_t *gl = (gl_t*)data;

   if (gl->fbo_inited)
   {
      glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
      glDeleteFramebuffers(gl->fbo_pass, gl->fbo);
      memset(gl->fbo_texture, 0, sizeof(gl->fbo_texture));
      memset(gl->fbo, 0, sizeof(gl->fbo));
      gl->fbo_inited = false;
      gl->fbo_pass = 0;
   }
}

void gl_init_fbo(void *data, unsigned width, unsigned height)
{
   int i;
   gl_t *gl = (gl_t*)data;

   if (gl_shader_num(gl) == 0)
      return;

   struct gfx_fbo_scale scale, scale_last;
   gl_shader_scale(gl, 1, &scale);
   gl_shader_scale(gl, gl_shader_num(gl), &scale_last);

   /* we always want FBO to be at least initialized on startup for consoles */
   if (gl_shader_num(gl) == 1 && !scale.valid)
      return;

   if (!check_fbo_proc(gl))
   {
      RARCH_ERR("Failed to locate FBO functions. Won't be able to use render-to-texture.\n");
      return;
   }

   gl->fbo_pass = gl_shader_num(gl) - 1;
   if (scale_last.valid)
      gl->fbo_pass++;

   if (!scale.valid)
   {
      scale.scale_x = 1.0f;
      scale.scale_y = 1.0f; 
      scale.type_x  = scale.type_y = RARCH_SCALE_INPUT;
      scale.valid   = true;
   }

   gl->fbo_scale[0] = scale;

   for (i = 1; i < gl->fbo_pass; i++)
   {
      gl_shader_scale(gl, i + 1, &gl->fbo_scale[i]);

      if (!gl->fbo_scale[i].valid)
      {
         gl->fbo_scale[i].scale_x = gl->fbo_scale[i].scale_y = 1.0f;
         gl->fbo_scale[i].type_x  = gl->fbo_scale[i].type_y  = RARCH_SCALE_INPUT;
         gl->fbo_scale[i].valid   = true;
      }
   }

   gl_compute_fbo_geometry(gl, width, height, gl->win_width, gl->win_height);

   for (i = 0; i < gl->fbo_pass; i++)
   {
      gl->fbo_rect[i].width  = next_pow2(gl->fbo_rect[i].img_width);
      gl->fbo_rect[i].height = next_pow2(gl->fbo_rect[i].img_height);
      RARCH_LOG("Creating FBO %d @ %ux%u\n", i, gl->fbo_rect[i].width, gl->fbo_rect[i].height);
   }

   gl_create_fbo_textures(gl);
   if (!gl_create_fbo_targets(gl))
   {
      glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
      RARCH_ERR("Failed to create FBO targets. Will continue without FBO.\n");
      return;
   }

   gl->fbo_inited = true;
}

#ifndef HAVE_GCMGL
static void gl_deinit_hw_render(gl_t *gl)
{
   if (gl->hw_render_fbo_init)
      glDeleteFramebuffers(gl->textures, gl->hw_render_fbo);
   if (gl->hw_render_depth_init)
      glDeleteRenderbuffers(gl->textures, gl->hw_render_depth);
   gl->hw_render_fbo_init = false;
}

static bool gl_init_hw_render(gl_t *gl, unsigned width, unsigned height)
{
   unsigned i;
   RARCH_LOG("[GL]: Initializing HW render (%u x %u).\n", width, height);
   GLint max_fbo_size = 0;
   GLint max_renderbuffer_size = 0;
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_fbo_size);
   glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer_size);
   RARCH_LOG("[GL]: Max texture size: %d px, renderbuffer size: %u px.\n", max_fbo_size, max_renderbuffer_size);

   if (!check_fbo_proc(gl))
      return false;

   glBindTexture(GL_TEXTURE_2D, 0);
   glGenFramebuffers(gl->textures, gl->hw_render_fbo);

   bool depth = g_extern.system.hw_render_callback.depth;
   bool stencil = g_extern.system.hw_render_callback.stencil;

#ifdef HAVE_OPENGLES2
   if (stencil && !gl_query_extension(gl, "OES_packed_depth_stencil"))
      return false;
#endif

   if (depth)
   {
      glGenRenderbuffers(gl->textures, gl->hw_render_depth);
      gl->hw_render_depth_init = true;
   }

   for (i = 0; i < gl->textures; i++)
   {
      glBindFramebuffer(GL_FRAMEBUFFER, gl->hw_render_fbo[i]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->texture[i], 0);

      if (depth)
      {
         if (stencil)
         {
#ifdef HAVE_OPENGLES2
            // GLES2 is a bit weird, as always. :P
            glBindRenderbuffer(GL_RENDERBUFFER, gl->hw_render_depth[i]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES,
                  width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);

            // There's no GL_DEPTH_STENCIL_ATTACHMENT like in desktop GL.
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                  GL_RENDERBUFFER, gl->hw_render_depth[i]);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                  GL_RENDERBUFFER, gl->hw_render_depth[i]);
#else
            // We use ARB FBO extensions, no need to check.
            glBindRenderbuffer(GL_RENDERBUFFER, gl->hw_render_depth[i]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                  width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                  GL_RENDERBUFFER, gl->hw_render_depth[i]);
#endif
         }
         else
         {
            glBindRenderbuffer(GL_RENDERBUFFER, gl->hw_render_depth[i]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                  width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                  GL_RENDERBUFFER, gl->hw_render_depth[i]);
         }
      }

      GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE)
      {
         RARCH_ERR("[GL]: Failed to create HW render FBO #%u, error: 0x%u.\n", i, (unsigned)status);
         return false;
      }
   }

   gl_bind_backbuffer();
   gl->hw_render_fbo_init = true;
   return true;
}
#endif
#endif

void gl_set_projection(void *data, struct gl_ortho *ortho, bool allow_rotate)
{
   gl_t *gl = (gl_t*)data;

   // Calculate projection.
   matrix_ortho(&gl->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (allow_rotate)
   {
      math_matrix rot;
      matrix_rotate_z(&rot, M_PI * gl->rotation / 180.0f);
      matrix_multiply(&gl->mvp, &rot, &gl->mvp_no_rot);
   }
   else
      gl->mvp = gl->mvp_no_rot;
}

void gl_set_viewport(void *data, unsigned width, unsigned height, bool force_full, bool allow_rotate)
{
   gl_t *gl = (gl_t*)data;

   int x = 0, y = 0;
   struct gl_ortho ortho = {0, 1, 0, 1, -1, 1};

   float device_aspect = 0.0f;
   if (gl->ctx_driver->translate_aspect)
      device_aspect = context_translate_aspect_func(gl, width, height);
   else
      device_aspect = (float)width / height;

   if (g_settings.video.scale_integer && !force_full)
   {
      gfx_scale_integer(&gl->vp, width, height, g_extern.system.aspect_ratio, gl->keep_aspect);
      width  = gl->vp.width;
      height = gl->vp.height;
   }
   else if (gl->keep_aspect && !force_full)
   {
      float desired_aspect = g_extern.system.aspect_ratio;
      float delta;

#if defined(HAVE_MENU)
      if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const struct rarch_viewport *custom =
            &g_extern.console.screen.viewports.custom_vp;

         // GL has bottom-left origin viewport.
         x      = custom->x;
         y      = gl->win_height - custom->y - custom->height;
         width  = custom->width;
         height = custom->height;
      }
      else
#endif
      {
         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), 
            // assume they are actually equal.
         }
         else if (device_aspect > desired_aspect)
         {
            delta = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
            x     = (int)roundf(width * (0.5f - delta));
            width = (unsigned)roundf(2.0f * width * delta);
         }
         else
         {
            delta  = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
            y      = (int)roundf(height * (0.5f - delta));
            height = (unsigned)roundf(2.0f * height * delta);
         }
      }

      gl->vp.x      = x;
      gl->vp.y      = y;
      gl->vp.width  = width;
      gl->vp.height = height;
   }
   else
   {
      gl->vp.x = gl->vp.y = 0;
      gl->vp.width = width;
      gl->vp.height = height;
   }

#if defined(RARCH_MOBILE)
   // In portrait mode, we want viewport to gravitate to top of screen.
   if (device_aspect < 1.0f)
      gl->vp.y *= 2;
#endif

   glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
   gl_set_projection(gl, &ortho, allow_rotate);

   // Set last backbuffer viewport.
   if (!force_full)
   {
      gl->vp_out_width  = width;
      gl->vp_out_height = height;
   }

   //RARCH_LOG("Setting viewport @ %ux%u\n", width, height);
}

static void gl_set_rotation(void *data, unsigned rotation)
{
   gl_t *gl = (gl_t*)data;
   struct gl_ortho ortho = {0, 1, 0, 1, -1, 1};

   gl->rotation = 90 * rotation;
   gl_set_projection(gl, &ortho, true);
}

#ifdef HAVE_FBO
static inline void gl_start_frame_fbo(void *data)
{
   gl_t *gl = (gl_t*)data;

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   glBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[0]);
   gl_set_viewport(gl, gl->fbo_rect[0].img_width, gl->fbo_rect[0].img_height, true, false);

   // Need to preserve the "flipped" state when in FBO as well to have 
   // consistent texture coordinates.
   // We will "flip" it in place on last pass.
   gl->coords.vertex = vertexes;
}

static void gl_check_fbo_dimensions(void *data)
{
   int i;
   gl_t *gl = (gl_t*)data;

   // Check if we have to recreate our FBO textures.
   for (i = 0; i < gl->fbo_pass; i++)
   {
      // Check proactively since we might suddently get sizes of tex_w width or tex_h height.
      if (gl->fbo_rect[i].max_img_width > gl->fbo_rect[i].width ||
            gl->fbo_rect[i].max_img_height > gl->fbo_rect[i].height)
      {
         unsigned img_width = gl->fbo_rect[i].max_img_width;
         unsigned img_height = gl->fbo_rect[i].max_img_height;
         unsigned max = img_width > img_height ? img_width : img_height;
         unsigned pow2_size = next_pow2(max);
         gl->fbo_rect[i].width = gl->fbo_rect[i].height = pow2_size;

         glBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[i]);
         glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i]);

         glTexImage2D(GL_TEXTURE_2D,
               0, RARCH_GL_INTERNAL_FORMAT32, gl->fbo_rect[i].width, gl->fbo_rect[i].height,
               0, RARCH_GL_TEXTURE_TYPE32,
               RARCH_GL_FORMAT32, NULL);

         glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

         GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
         if (status != GL_FRAMEBUFFER_COMPLETE)
            RARCH_WARN("Failed to reinit FBO texture.\n");

         RARCH_LOG("Recreating FBO texture #%d: %ux%u\n", i, gl->fbo_rect[i].width, gl->fbo_rect[i].height);
      }
   }
}

static void gl_frame_fbo(void *data, const struct gl_tex_info *tex_info)
{
   int i;
   gl_t *gl = (gl_t*)data;
   GLfloat fbo_tex_coords[8] = {0.0f};

   // Render the rest of our passes.
   gl->coords.tex_coord = fbo_tex_coords;

   // It's kinda handy ... :)
   const struct gl_fbo_rect *prev_rect;
   const struct gl_fbo_rect *rect;
   struct gl_tex_info *fbo_info;

   struct gl_tex_info fbo_tex_info[MAX_SHADERS];
   unsigned fbo_tex_info_cnt = 0;

   // Calculate viewports, texture coordinates etc, and render all passes from FBOs, to another FBO.
   for (i = 1; i < gl->fbo_pass; i++)
   {
      prev_rect = &gl->fbo_rect[i - 1];
      rect = &gl->fbo_rect[i];
      fbo_info = &fbo_tex_info[i - 1];

      GLfloat xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
      GLfloat yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

      set_texture_coords(fbo_tex_coords, xamt, yamt);

      fbo_info->tex = gl->fbo_texture[i - 1];
      fbo_info->input_size[0] = prev_rect->img_width;
      fbo_info->input_size[1] = prev_rect->img_height;
      fbo_info->tex_size[0] = prev_rect->width;
      fbo_info->tex_size[1] = prev_rect->height;
      memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));

      glBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[i]);

      if (gl->shader)
         gl->shader->use(gl, i + 1);
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i - 1]);

      glClear(GL_COLOR_BUFFER_BIT);

      // Render to FBO with certain size.
      gl_set_viewport(gl, rect->img_width, rect->img_height, true, false);
      if (gl->shader)
         gl->shader->set_params(gl, prev_rect->img_width, prev_rect->img_height, 
            prev_rect->width, prev_rect->height, 
            gl->vp.width, gl->vp.height, g_extern.frame_count, 
            tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

      gl_shader_set_coords(gl, &gl->coords, &gl->mvp);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      fbo_tex_info_cnt++;
   }

   // Render our last FBO texture directly to screen.
   prev_rect = &gl->fbo_rect[gl->fbo_pass - 1];
   GLfloat xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
   GLfloat yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

   set_texture_coords(fbo_tex_coords, xamt, yamt);

   // Render our FBO texture to back buffer.
   gl_bind_backbuffer();
   if (gl->shader)
      gl->shader->use(gl, gl->fbo_pass + 1);

   glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[gl->fbo_pass - 1]);

   glClear(GL_COLOR_BUFFER_BIT);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);

   if (gl->shader)
      gl->shader->set_params(gl, prev_rect->img_width, prev_rect->img_height, 
         prev_rect->width, prev_rect->height, 
         gl->vp.width, gl->vp.height, g_extern.frame_count, 
         tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

   gl->coords.vertex = gl->vertex_ptr;

   gl_shader_set_coords(gl, &gl->coords, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   gl->coords.tex_coord = gl->tex_coords;
}
#endif

static void gl_update_resize(void *data)
{
   gl_t *gl = (gl_t*)data;
#ifdef HAVE_FBO
   if (!gl->fbo_inited)
      gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
   else
   {
      gl_check_fbo_dimensions(gl);

      // Go back to what we're supposed to do, render to FBO #0 :D
      gl_start_frame_fbo(gl);
   }
#else
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
#endif
}

static void gl_update_input_size(void *data, unsigned width, unsigned height, unsigned pitch, bool clear)
{
   gl_t *gl = (gl_t*)data;
   // Res change. Need to clear out texture.
   if ((width != gl->last_width[gl->tex_index] || height != gl->last_height[gl->tex_index]) && gl->empty_buf)
   {
      gl->last_width[gl->tex_index] = width;
      gl->last_height[gl->tex_index] = height;

      if (clear)
      {
         glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(width * sizeof(uint32_t)));
#if defined(HAVE_PSGL)
         glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
               gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size,
               gl->tex_w * gl->tex_h * gl->base_size,
               gl->empty_buf);
#else
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, gl->tex_w, gl->tex_h, gl->texture_type,
               gl->texture_fmt, gl->empty_buf);
#endif
      }

      GLfloat xamt = (GLfloat)width / gl->tex_w;
      GLfloat yamt = (GLfloat)height / gl->tex_h;

      set_texture_coords(gl->tex_coords, xamt, yamt);
   }
   // We might have used different texture coordinates last frame. Edge case if resolution changes very rapidly.
   else if (width != gl->last_width[(gl->tex_index + gl->textures - 1) % gl->textures] ||
         height != gl->last_height[(gl->tex_index + gl->textures - 1) % gl->textures])
   {
      GLfloat xamt = (GLfloat)width / gl->tex_w;
      GLfloat yamt = (GLfloat)height / gl->tex_h;
      set_texture_coords(gl->tex_coords, xamt, yamt);
   }
}

// It is *much* faster (order of mangnitude on my setup) to use a custom SIMD-optimized conversion routine than letting GL do it :(
#if !defined(HAVE_PSGL) && !defined(HAVE_OPENGLES2)
static inline void gl_convert_frame_rgb16_32(void *data, void *output, const void *input, int width, int height, int in_pitch)
{
   gl_t *gl = (gl_t*)data;
   if (width != gl->scaler.in_width || height != gl->scaler.in_height)
   {
      gl->scaler.in_width    = width;
      gl->scaler.in_height   = height;
      gl->scaler.out_width   = width;
      gl->scaler.out_height  = height;
      gl->scaler.in_fmt      = SCALER_FMT_RGB565;
      gl->scaler.out_fmt     = SCALER_FMT_ARGB8888;
      gl->scaler.scaler_type = SCALER_TYPE_POINT;
      scaler_ctx_gen_filter(&gl->scaler);
   }

   gl->scaler.in_stride  = in_pitch;
   gl->scaler.out_stride = width * sizeof(uint32_t);
   scaler_ctx_scale(&gl->scaler, output, input);
}
#endif

#ifdef HAVE_OPENGLES2
static inline void gl_convert_frame_argb8888_abgr8888(void *data, void *output, const void *input,
      int width, int height, int in_pitch)
{
   gl_t *gl = (gl_t*)data;
   if (width != gl->scaler.in_width || height != gl->scaler.in_height)
   {
      gl->scaler.in_width    = width;
      gl->scaler.in_height   = height;
      gl->scaler.out_width   = width;
      gl->scaler.out_height  = height;
      gl->scaler.in_fmt      = SCALER_FMT_ARGB8888;
      gl->scaler.out_fmt     = SCALER_FMT_ABGR8888;
      gl->scaler.scaler_type = SCALER_TYPE_POINT;
      scaler_ctx_gen_filter(&gl->scaler);
   }

   gl->scaler.in_stride  = in_pitch;
   gl->scaler.out_stride = width * sizeof(uint32_t);
   scaler_ctx_scale(&gl->scaler, output, input);
}
#endif

static void gl_init_textures_data(void *data)
{
   unsigned i;
   gl_t *gl = (gl_t*)data;
   for (i = 0; i < gl->textures; i++)
   {
      gl->last_width[i]  = gl->tex_w;
      gl->last_height[i] = gl->tex_h;
   }

   for (i = 0; i < gl->textures; i++)
   {
      gl->prev_info[i].tex           = gl->texture[0];
      gl->prev_info[i].input_size[0] = gl->tex_w;
      gl->prev_info[i].tex_size[0]   = gl->tex_w;
      gl->prev_info[i].input_size[1] = gl->tex_h;
      gl->prev_info[i].tex_size[1]   = gl->tex_h;
      memcpy(gl->prev_info[i].coord, tex_coords, sizeof(tex_coords)); 
   }
}

static void gl_init_textures(void *data, const video_info_t *video)
{
   unsigned i;
   gl_t *gl = (gl_t*)data;
#if defined(HAVE_EGL) && defined(HAVE_OPENGLES2)
   // Use regular textures if we use HW render.
   gl->egl_images = !gl->hw_render_use && check_eglimage_proc() &&
      gl->ctx_driver->init_egl_image_buffer && context_init_egl_image_buffer_func(gl, video);
#else
   (void)video;
#endif

#ifdef HAVE_PSGL
   if (!gl->pbo)
      glGenBuffers(1, &gl->pbo);

   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, gl->pbo);
   glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
         gl->tex_w * gl->tex_h * gl->base_size * gl->textures, NULL, GL_STREAM_DRAW);
#endif

   GLenum internal_fmt = gl->internal_fmt;
#ifndef HAVE_PSGL
   GLenum texture_type = gl->texture_type;
   GLenum texture_fmt  = gl->texture_fmt;
#endif

   // GLES is picky about which format we use here.
   // Without extensions, we can *only* render to 16-bit FBOs.
#ifdef HAVE_OPENGLES2
   if (gl->hw_render_use && gl->base_size == sizeof(uint32_t))
   {
      bool support_argb = gl_query_extension(gl, "OES_rgb8_rgba8") || gl_query_extension(gl, "ARM_argb8");
      if (support_argb)
      {
         internal_fmt = GL_RGBA;
         texture_type = GL_RGBA;
         texture_fmt  = GL_UNSIGNED_BYTE;
      }
      else
      {
         RARCH_WARN("[GL]: 32-bit FBO not supported. Falling back to 16-bit.\n");
         internal_fmt = GL_RGB;
         texture_type = GL_RGB;
         texture_fmt  = GL_UNSIGNED_SHORT_5_6_5;
      }
   }
#endif

   glGenTextures(gl->textures, gl->texture);

   for (i = 0; i < gl->textures; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->texture[i]);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->wrap_mode);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->wrap_mode);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_filter);

#ifdef HAVE_PSGL
      glTextureReferenceSCE(GL_TEXTURE_2D, 1,
            gl->tex_w, gl->tex_h, 0, 
            internal_fmt,
            gl->tex_w * gl->base_size,
            gl->tex_w * gl->tex_h * i * gl->base_size);
#else
      if (!gl->egl_images)
      {
         glTexImage2D(GL_TEXTURE_2D,
               0, internal_fmt, gl->tex_w, gl->tex_h, 0, texture_type,
               texture_fmt, gl->empty_buf ? gl->empty_buf : NULL);
      }
#endif
   }
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

static inline void gl_copy_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch)
{
   gl_t *gl = (gl_t*)data;
#if defined(HAVE_OPENGLES2)
#if defined(HAVE_EGL)
   if (gl->egl_images)
   {
      EGLImageKHR img = 0;
      bool new_egl = context_write_egl_image_func(gl, frame, width, height, pitch, (gl->base_size == 4), gl->tex_index, &img);

      if (img == EGL_NO_IMAGE_KHR)
      {
         RARCH_ERR("[GL]: Failed to create EGL image.\n");
         return;
      }

      if (new_egl)
         glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)img);
   }
   else
#endif
   {
      glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(width * gl->base_size));

      // Fallback for GLES devices without GL_BGRA_EXT.
      if (gl->base_size == 4 && driver.gfx_use_rgba)
      {
         gl_convert_frame_argb8888_abgr8888(gl, gl->conv_buffer, frame, width, height, pitch);
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, gl->conv_buffer);
      }
      else if (gl->support_unpack_row_length)
      {
         glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, frame);

         glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
      }
      else
      {
         // No GL_UNPACK_ROW_LENGTH ;(
         unsigned pitch_width = pitch / gl->base_size;
         if (width == pitch_width) // Happy path :D
         {
            glTexSubImage2D(GL_TEXTURE_2D,
                  0, 0, 0, width, height, gl->texture_type,
                  gl->texture_fmt, frame);
         }
         else // Slower path.
         {
            unsigned h;
            const unsigned line_bytes = width * gl->base_size;

            uint8_t *dst = (uint8_t*)gl->conv_buffer; // This buffer is preallocated for this purpose.
            const uint8_t *src = (const uint8_t*)frame;

            for (h = 0; h < height; h++, src += pitch, dst += line_bytes)
               memcpy(dst, src, line_bytes);

            glTexSubImage2D(GL_TEXTURE_2D,
                  0, 0, 0, width, height, gl->texture_type,
                  gl->texture_fmt, gl->conv_buffer);         
         }
      }
   }
#elif defined(HAVE_PSGL)
   unsigned h;
   size_t buffer_addr        = gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size;
   size_t buffer_stride      = gl->tex_w * gl->base_size;
   const uint8_t *frame_copy = frame;
   size_t frame_copy_size    = width * gl->base_size;

   uint8_t *buffer = (uint8_t*)glMapBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, GL_READ_WRITE) + buffer_addr;
   for (h = 0; h < height; h++, buffer += buffer_stride, frame_copy += pitch)
      memcpy(buffer, frame_copy, frame_copy_size);

   glUnmapBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE);
#else
   glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(pitch));
   if (gl->base_size == 2)
   {
      const void *buf = frame;
      if (!gl->have_es2_compat)
      {
         // Convert to 32-bit textures on desktop GL.
         gl_convert_frame_rgb16_32(gl, gl->conv_buffer, frame, width, height, pitch);
         buf = gl->conv_buffer;
      }
      else
         glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);

      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, width, height, gl->texture_type,
            gl->texture_fmt, buf);
   }
   else
   {
      glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);
      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, width, height, gl->texture_type,
            gl->texture_fmt, frame);
   }

   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
}

static inline void gl_set_prev_texture(void *data, const struct gl_tex_info *tex_info)
{
   gl_t *gl = (gl_t*)data;
   memmove(gl->prev_info + 1, gl->prev_info, sizeof(*tex_info) * (gl->textures - 1));
   memcpy(&gl->prev_info[0], tex_info, sizeof(*tex_info));
}

static inline void gl_set_shader_viewport(void *data, unsigned shader)
{
   gl_t *gl = (gl_t*)data;
   if (gl->shader)
      gl->shader->use(gl, shader);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
}

#if !defined(HAVE_OPENGLES) && defined(HAVE_FFMPEG)
static void gl_pbo_async_readback(void *data)
{
   gl_t *gl = (gl_t*)data;
   glBindBuffer(GL_PIXEL_PACK_BUFFER, gl->pbo_readback[gl->pbo_readback_index++]);
   gl->pbo_readback_index &= 3;

   // If set, we 3 rendered frames already buffered up.
   gl->pbo_readback_valid |= gl->pbo_readback_index == 0;

   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
   glPixelStorei(GL_PACK_ALIGNMENT, get_alignment(gl->vp.width * sizeof(uint32_t)));

   // Read asynchronously into PBO buffer.
   RARCH_PERFORMANCE_INIT(async_readback);
   RARCH_PERFORMANCE_START(async_readback);
   glReadBuffer(GL_BACK);
   glReadPixels(gl->vp.x, gl->vp.y,
         gl->vp.width, gl->vp.height,
         GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
   RARCH_PERFORMANCE_STOP(async_readback);

   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}
#endif

#if defined(HAVE_MENU)
static inline void gl_draw_texture(void *data)
{
   gl_t *gl = (gl_t*)data;

   if (!gl->rgui_texture)
      return;

   const GLfloat color[] = {
      1.0f, 1.0f, 1.0f, gl->rgui_texture_alpha,
      1.0f, 1.0f, 1.0f, gl->rgui_texture_alpha,
      1.0f, 1.0f, 1.0f, gl->rgui_texture_alpha,
      1.0f, 1.0f, 1.0f, gl->rgui_texture_alpha,
   };

   gl->coords.vertex = vertexes_flipped;
   gl->coords.tex_coord = tex_coords;
   gl->coords.color = color;
   glBindTexture(GL_TEXTURE_2D, gl->rgui_texture);

   if (gl->shader)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp_no_rot);

   glEnable(GL_BLEND);

   if (gl->rgui_texture_full_screen)
   {
      glViewport(0, 0, gl->win_width, gl->win_height);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
   }
   else
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   glDisable(GL_BLEND);

   gl->coords.vertex = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color = gl->white_color_ptr;
}
#endif

static bool gl_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   RARCH_PERFORMANCE_INIT(frame_run);
   RARCH_PERFORMANCE_START(frame_run);

   gl_t *gl = (gl_t*)data;

#ifndef HAVE_OPENGLES
   if (gl->core_context)
      glBindVertexArray(gl->vao);
#endif

   if (gl->shader)
      gl->shader->use(gl, 1);

#ifdef IOS // Apparently the viewport is lost each frame, thanks apple.
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
#endif

#ifdef HAVE_FBO
   // Render to texture in first pass.
   if (gl->fbo_inited)
   {
      // Recompute FBO geometry.
      // When width/height changes or window sizes change, we have to recalcuate geometry of our FBO.
      gl_compute_fbo_geometry(gl, width, height, gl->vp_out_width, gl->vp_out_height);
      gl_start_frame_fbo(gl);
   }
#endif

   if (gl->should_resize)
   {
      gl->should_resize = false;
      context_set_resize_func(gl, gl->win_width, gl->win_height);

      // On resize, we might have to recreate our FBOs due to "Viewport" scale, and set a new viewport.
      gl_update_resize(gl);
   }

   if (frame) // Can be NULL for frame dupe / NULL render.
   {
      gl->tex_index = (gl->tex_index + 1) % gl->textures;
      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

#ifdef HAVE_FBO
      if (!gl->hw_render_fbo_init)
#endif
      {
         gl_update_input_size(gl, width, height, pitch, true);
         RARCH_PERFORMANCE_INIT(copy_frame);
         RARCH_PERFORMANCE_START(copy_frame);
         gl_copy_frame(gl, frame, width, height, pitch);
         RARCH_PERFORMANCE_STOP(copy_frame);
      }
   }
   else
      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   // Have to reset rendering state which libretro core could easily have overridden.
#ifdef HAVE_FBO
   if (gl->hw_render_fbo_init)
   {
      gl_update_input_size(gl, width, height, pitch, false);
      if (!gl->fbo_inited)
      {
         gl_bind_backbuffer();
         gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
      }

#ifndef HAVE_OPENGLES
      if (!gl->core_context)
         glEnable(GL_TEXTURE_2D);
#endif
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_STENCIL_TEST);
      glDisable(GL_CULL_FACE);
      glDisable(GL_DITHER);
      glDisable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   }
#endif

   struct gl_tex_info tex_info = {0};
   tex_info.tex           = gl->texture[gl->tex_index];
   tex_info.input_size[0] = width;
   tex_info.input_size[1] = height;
   tex_info.tex_size[0]   = gl->tex_w;
   tex_info.tex_size[1]   = gl->tex_h;

   memcpy(tex_info.coord, gl->tex_coords, sizeof(gl->tex_coords));

   glClear(GL_COLOR_BUFFER_BIT);
   if (g_settings.video.black_frame_insertion)
   {
      context_swap_buffers_func(gl);
      glClear(GL_COLOR_BUFFER_BIT);
   }

   if (gl->shader)
      gl->shader->set_params(gl, width, height,
         gl->tex_w, gl->tex_h,
         gl->vp.width, gl->vp.height,
         g_extern.frame_count, 
         &tex_info, gl->prev_info, NULL, 0);

   gl_shader_set_coords(gl, &gl->coords, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

#ifdef HAVE_FBO
   if (gl->fbo_inited)
      gl_frame_fbo(gl, &tex_info);
#endif

   gl_set_prev_texture(gl, &tex_info);

#if defined(HAVE_MENU)
   if (gl->rgui_texture_enable)
      gl_draw_texture(gl);
#endif

   if (msg && gl->font_ctx)
      gl->font_ctx->render_msg(gl, msg, NULL);

#ifdef HAVE_OVERLAY
   if (gl->overlay_enable)
      gl_render_overlay(gl);
#endif

   context_update_window_title_func(gl);

   RARCH_PERFORMANCE_STOP(frame_run);

#ifdef HAVE_FBO
   // Reset state which could easily mess up libretro core.
   if (gl->hw_render_fbo_init)
   {
      if (gl->shader)
         gl->shader->use(gl, 0);
      glBindTexture(GL_TEXTURE_2D, 0);
#ifndef NO_GL_FF_VERTEX
      gl_disable_client_arrays(gl);
#endif
   }
#endif

#ifndef NO_GL_READ_PIXELS
   // Screenshots.
   if (gl->readback_buffer_screenshot)
   {
      glPixelStorei(GL_PACK_ALIGNMENT, 4);
#ifndef HAVE_OPENGLES
      glPixelStorei(GL_PACK_ROW_LENGTH, 0);
      glReadBuffer(GL_BACK);
#endif
      glReadPixels(gl->vp.x, gl->vp.y,
            gl->vp.width, gl->vp.height,
            GL_RGBA, GL_UNSIGNED_BYTE, gl->readback_buffer_screenshot);
   }
#if !defined(HAVE_OPENGLES) && defined(HAVE_FFMPEG)
   else if (gl->pbo_readback_enable)
      gl_pbo_async_readback(gl);
#endif
#endif

   context_swap_buffers_func(gl);
   g_extern.frame_count++;

#ifdef HAVE_GL_SYNC
   if (g_settings.video.hard_sync && gl->have_sync)
   {
      RARCH_PERFORMANCE_INIT(gl_fence);
      RARCH_PERFORMANCE_START(gl_fence);
      glClear(GL_COLOR_BUFFER_BIT);
      gl->fences[gl->fence_count++] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

      while (gl->fence_count > g_settings.video.hard_sync_frames)
      {
         glClientWaitSync(gl->fences[0], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
         glDeleteSync(gl->fences[0]);

         gl->fence_count--;
         memmove(gl->fences, gl->fences + 1, gl->fence_count * sizeof(GLsync));
      }

      RARCH_PERFORMANCE_STOP(gl_fence);
   }
#endif

#ifndef HAVE_OPENGLES
   if (gl->core_context)
      glBindVertexArray(0);
#endif

   return true;
}

#ifdef HAVE_OVERLAY
static void gl_free_overlay(gl_t *gl)
{
   unsigned i;
   for (i = 0; i < gl->overlays; i++)
      if (gl->overlay[i].tex)
         glDeleteTextures(1, &gl->overlay[i].tex);

   free(gl->overlay);
   gl->overlay = NULL;
   gl->overlays = 0;
}
#endif

static void gl_free(void *data)
{
#ifdef RARCH_CONSOLE
   if (driver.video_data)
      return;
#endif

   gl_t *gl = (gl_t*)data;

#ifdef HAVE_GL_SYNC
   if (gl->have_sync)
   {
      unsigned i;
      for (i = 0; i < gl->fence_count; i++)
      {
         glClientWaitSync(gl->fences[i], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
         glDeleteSync(gl->fences[i]);
      }
      gl->fence_count = 0;
   }
#endif

   if (gl->font_ctx)
      gl->font_ctx->deinit(gl);
   gl_shader_deinit(gl);

#ifndef NO_GL_FF_VERTEX
   gl_disable_client_arrays(gl);
#endif

   glDeleteTextures(gl->textures, gl->texture);

#if defined(HAVE_MENU)
   if (gl->rgui_texture)
      glDeleteTextures(1, &gl->rgui_texture);
#endif

#ifdef HAVE_OVERLAY
   gl_free_overlay(gl);
#endif

#if defined(HAVE_PSGL)
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
   glDeleteBuffers(1, &gl->pbo);
#endif

   scaler_ctx_gen_reset(&gl->scaler);

#if !defined(HAVE_OPENGLES) && defined(HAVE_FFMPEG)
   if (gl->pbo_readback_enable)
   {
      glDeleteBuffers(4, gl->pbo_readback);
      scaler_ctx_gen_reset(&gl->pbo_readback_scaler);
   }
#endif

#ifdef HAVE_FBO
   gl_deinit_fbo(gl);
#ifndef HAVE_GCMGL
   gl_deinit_hw_render(gl);
#endif
#endif

#ifndef HAVE_OPENGLES
   if (gl->core_context)
   {
      glBindVertexArray(0);
      glDeleteVertexArrays(1, &gl->vao);
   }
#endif

   context_destroy_func(gl);

   free(gl->empty_buf);
   free(gl->conv_buffer);
   free(gl);
}

static void gl_set_nonblock_state(void *data, bool state)
{
   RARCH_LOG("GL VSync => %s\n", state ? "off" : "on");

   gl_t *gl = (gl_t*)data;
   context_swap_interval_func(gl, state ? 0 : g_settings.video.swap_interval);
}

static bool resolve_extensions(gl_t *gl)
{
#ifndef HAVE_OPENGLES
   gl->core_context = g_extern.system.hw_render_callback.context_type == RETRO_HW_CONTEXT_OPENGL_CORE;
   if (gl->core_context)
      RARCH_LOG("[GL]: Using Core GL context.\n");
   if (gl->core_context &&
         !init_vao(gl))
   {
      RARCH_ERR("[GL]: Failed to init VAOs.\n");
      return false;
   }

   // GL_RGB565 internal format support.
   // Even though es2 support is claimed, the format is not supported on older ATI catalyst drivers.
   // The speed gain from using GL_RGB565 is worth adding some workarounds for.
   const char *vendor = (const char*)glGetString(GL_VENDOR);
   const char *renderer = (const char*)glGetString(GL_RENDERER);
   if (vendor && renderer && (strstr(vendor, "ATI") || strstr(renderer, "ATI")))
      RARCH_LOG("[GL]: ATI card detected, skipping check for GL_RGB565 support.\n");
   else
      gl->have_es2_compat = gl_query_extension(gl, "ARB_ES2_compatibility");
#endif

#ifdef HAVE_GL_SYNC
   gl->have_sync = check_sync_proc(gl);
   if (gl->have_sync && g_settings.video.hard_sync)
      RARCH_LOG("[GL]: Using ARB_sync to reduce latency.\n");
#endif

   driver.gfx_use_rgba = false;
#ifdef HAVE_OPENGLES2
   if (gl_query_extension(gl, "BGRA8888")) // There are both APPLE and EXT variants.
      RARCH_LOG("[GL]: BGRA8888 extension found for GLES.\n");
   else
   {
      driver.gfx_use_rgba = true;
      RARCH_WARN("[GL]: GLES implementation does not have BGRA8888 extension.\n"
                 "32-bit path will require conversion.\n");
   }

   gl->support_unpack_row_length = false;
   if (gl_query_extension(gl, "GL_EXT_unpack_subimage"))
   {
      RARCH_LOG("[GL]: Extension GL_EXT_unpack_subimage, can copy textures faster using UNPACK_ROW_LENGTH.\n");
      gl->support_unpack_row_length = true;
   }
   gl->has_fp_fbo = gl_query_extension(gl, "OES_texture_float_linear");
#else
#ifdef HAVE_FBO
   // Float FBO is core in 3.2.
   gl->has_fp_fbo = gl->core_context || gl_query_extension(gl, "ARB_texture_float");
#endif
#endif

#ifdef GL_DEBUG
   // Useful for debugging, but kinda obnoxious otherwise.
   RARCH_LOG("[GL]: Supported extensions:\n");
   if (gl->core_context)
   {
#ifdef GL_NUM_EXTENSIONS
      GLint exts = 0;
      glGetIntegerv(GL_NUM_EXTENSIONS, &exts);
      for (GLint i = 0; i < exts; i++)
      {
         const char *ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
         if (ext)
            RARCH_LOG("\t%s\n", ext);
      }
#endif
   }
   else
   {
      const char *ext = (const char*)glGetString(GL_EXTENSIONS);
      if (ext)
      {
         struct string_list *list = string_split(ext, " ");
         for (size_t i = 0; i < list->size; i++)
            RARCH_LOG("\t%s\n", list->elems[i].data);
         string_list_free(list);
      }
   }
#endif

   return true;
}

static inline void gl_set_texture_fmts(void *data, bool rgb32)
{
   gl_t *gl = (gl_t*)data;

   gl->internal_fmt = rgb32 ? RARCH_GL_INTERNAL_FORMAT32 : RARCH_GL_INTERNAL_FORMAT16;
   gl->texture_type = rgb32 ? RARCH_GL_TEXTURE_TYPE32 : RARCH_GL_TEXTURE_TYPE16;
   gl->texture_fmt  = rgb32 ? RARCH_GL_FORMAT32 : RARCH_GL_FORMAT16;
   gl->base_size    = rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

   if (driver.gfx_use_rgba && rgb32)
   {
      gl->internal_fmt = GL_RGBA;
      gl->texture_type = GL_RGBA;
   }

#ifndef HAVE_OPENGLES
   if (!rgb32 && gl->have_es2_compat) // Use GL_RGB565 instead.
   {
      RARCH_LOG("[GL]: Using GL_RGB565 for texture uploads.\n");
      gl->internal_fmt = RARCH_GL_INTERNAL_FORMAT16_565;
      gl->texture_type = RARCH_GL_TEXTURE_TYPE16_565;
      gl->texture_fmt = RARCH_GL_FORMAT16_565;
   }
#endif
}

#ifdef RARCH_CONSOLE
static inline void gl_reinit_textures(void *data, const video_info_t *video)
{
   gl_t *gl = (gl_t*)data;
   unsigned old_base_size = gl->base_size;
   unsigned old_width     = gl->tex_w;
   unsigned old_height    = gl->tex_h;

   gl_set_texture_fmts(gl, video->rgb32);
   gl->tex_w = gl->tex_h = RARCH_SCALE_BASE * video->input_scale;

   gl->empty_buf = realloc(gl->empty_buf, sizeof(uint32_t) * gl->tex_w * gl->tex_h);
   if (gl->empty_buf)
      memset(gl->empty_buf, 0, sizeof(uint32_t) * gl->tex_w * gl->tex_h);

   if (old_base_size != gl->base_size || old_width != gl->tex_w || old_height != gl->tex_h)
   {
      RARCH_LOG("Reinitializing textures (%u x %u @ %u bpp)\n", gl->tex_w, gl->tex_h, gl->base_size * CHAR_BIT);

#if defined(HAVE_PSGL)
      glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
      glDeleteBuffers(1, &gl->pbo);
      gl->pbo = 0;
#endif

      glBindTexture(GL_TEXTURE_2D, 0);
      glDeleteTextures(gl->textures, gl->texture);

      gl_init_textures(gl, video);
      gl_init_textures_data(gl);

#ifdef HAVE_FBO
      if (gl->tex_w > old_width || gl->tex_h > old_height)
      {
         RARCH_LOG("Reiniting FBO.\n");
         gl_deinit_fbo(gl);
         gl_init_fbo(gl, gl->tex_w, gl->tex_h);
      }
#endif
   }
   else
      RARCH_LOG("Reinitializing textures skipped.\n");

   if (!gl_check_error())
      RARCH_ERR("GL error reported while reinitializing textures. This should not happen ...\n");
}
#endif

#if !defined(HAVE_OPENGLES) && defined(HAVE_FFMPEG)
static void gl_init_pbo_readback(void *data)
{
   unsigned i;
   gl_t *gl = (gl_t*)data;
   // Only bother with this if we're doing FFmpeg GPU recording.
   gl->pbo_readback_enable = g_settings.video.gpu_record && g_extern.recording;
   if (!gl->pbo_readback_enable)
      return;

   RARCH_LOG("Async PBO readback enabled.\n");

   glGenBuffers(4, gl->pbo_readback);
   for (i = 0; i < 4; i++)
   {
      glBindBuffer(GL_PIXEL_PACK_BUFFER, gl->pbo_readback[i]);
      glBufferData(GL_PIXEL_PACK_BUFFER, gl->vp.width * gl->vp.height * sizeof(uint32_t),
            NULL, GL_STREAM_COPY);
   }
   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

   struct scaler_ctx *scaler = &gl->pbo_readback_scaler;
   scaler->in_width    = gl->vp.width;
   scaler->in_height   = gl->vp.height;
   scaler->out_width   = gl->vp.width;
   scaler->out_height  = gl->vp.height;
   scaler->in_stride   = gl->vp.width * sizeof(uint32_t);
   scaler->out_stride  = gl->vp.width * 3;
   scaler->in_fmt      = SCALER_FMT_ARGB8888;
   scaler->out_fmt     = SCALER_FMT_BGR24;
   scaler->scaler_type = SCALER_TYPE_POINT;

   if (!scaler_ctx_gen_filter(scaler))
   {
      gl->pbo_readback_enable = false;
      RARCH_ERR("Failed to init pixel conversion for PBO.\n");
      glDeleteBuffers(4, gl->pbo_readback);
   }
}
#endif

static const gfx_ctx_driver_t *gl_get_context(gl_t *gl)
{
   const struct retro_hw_render_callback *cb = &g_extern.system.hw_render_callback;
   unsigned major = cb->version_major;
   unsigned minor = cb->version_minor;
#ifdef HAVE_OPENGLES
   enum gfx_ctx_api api = GFX_CTX_OPENGL_ES_API;
   const char *api_name = "OpenGL ES 2.0";
#ifdef HAVE_OPENGLES3
   if (cb->context_type == RETRO_HW_CONTEXT_OPENGLES3)
   {
      major = 3;
      minor = 0;
      api_name = "OpenGL ES 3.0";
   }
#endif
#else
   enum gfx_ctx_api api = GFX_CTX_OPENGL_API;
   const char *api_name = "OpenGL";
#endif

   if (*g_settings.video.gl_context)
   {
      const gfx_ctx_driver_t *ctx = gfx_ctx_find_driver(g_settings.video.gl_context);
      if (ctx)
      {
         if (!ctx->bind_api(gl, api, major, minor))
         {
            RARCH_ERR("Failed to bind API %s to context %s.\n", api_name, g_settings.video.gl_context);
            return NULL;
         }

         if (!ctx->init(gl))
         {
            RARCH_ERR("Failed to init GL context: %s.\n", ctx->ident);
            return NULL;
         }
      }
      else
      {
         RARCH_ERR("Didn't find GL context: %s.\n", g_settings.video.gl_context);
         return NULL;
      }

      return ctx;
   }
   else
      return gfx_ctx_init_first(gl, api, major, minor);
}

#ifdef GL_DEBUG
#ifdef HAVE_OPENGLES2
#define DEBUG_CALLBACK_TYPE GL_APIENTRY
#else
#define DEBUG_CALLBACK_TYPE APIENTRY
#endif
static void DEBUG_CALLBACK_TYPE gl_debug_cb(GLenum source, GLenum type,
      GLuint id, GLenum severity, GLsizei length,
      const GLchar *message, void *userParam)
{
   (void)id;
   (void)length;

   gl_t *gl = (gl_t*)userParam; // Useful for debugger.
   (void)gl;

   const char *src;
   switch (source)
   {
      case GL_DEBUG_SOURCE_API: src = "API"; break;
      case GL_DEBUG_SOURCE_WINDOW_SYSTEM: src = "Window system"; break;
      case GL_DEBUG_SOURCE_SHADER_COMPILER: src = "Shader compiler"; break;
      case GL_DEBUG_SOURCE_THIRD_PARTY: src = "3rd party"; break;
      case GL_DEBUG_SOURCE_APPLICATION: src = "Application"; break;
      case GL_DEBUG_SOURCE_OTHER: src = "Other"; break;
      default: src = "Unknown"; break;
   }

   const char *typestr;
   switch (type)
   {
      case GL_DEBUG_TYPE_ERROR: typestr = "Error"; break;
      case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typestr = "Deprecated behavior"; break;
      case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: typestr = "Undefined behavior"; break;
      case GL_DEBUG_TYPE_PORTABILITY: typestr = "Portability"; break;
      case GL_DEBUG_TYPE_PERFORMANCE: typestr = "Performance"; break;
      case GL_DEBUG_TYPE_MARKER: typestr = "Marker"; break;
      case GL_DEBUG_TYPE_PUSH_GROUP: typestr = "Push group"; break;
      case GL_DEBUG_TYPE_POP_GROUP: typestr = "Pop group"; break;
      case GL_DEBUG_TYPE_OTHER: typestr = "Other"; break;
      default: typestr = "Unknown"; break;
   }

   switch (severity)
   {
      case GL_DEBUG_SEVERITY_HIGH:
         RARCH_ERR("[GL debug (High, %s, %s)]: %s\n", src, typestr, message);
         break;
      case GL_DEBUG_SEVERITY_MEDIUM:
         RARCH_WARN("[GL debug (Medium, %s, %s)]: %s\n", src, typestr, message);
         break;
      case GL_DEBUG_SEVERITY_LOW:
         RARCH_LOG("[GL debug (Low, %s, %s)]: %s\n", src, typestr, message);
         break;
   }
}

static void gl_begin_debug(gl_t *gl)
{
   if (gl_query_extension(gl, "KHR_debug"))
   {
      glDebugMessageCallback(gl_debug_cb, gl);
      glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
   }
#ifndef HAVE_OPENGLES2
   else if (gl_query_extension(gl, "ARB_debug_output"))
   {
      glDebugMessageCallbackARB(gl_debug_cb, gl);
      glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
   }
#endif
   else
      RARCH_ERR("Neither GL_KHR_debug nor GL_ARB_debug_output are implemented. Cannot start GL debugging.\n");
}
#endif

static void *gl_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
#ifdef _WIN32
   gfx_set_dwm();
#endif

#ifdef RARCH_CONSOLE
   if (driver.video_data)
   {
      gl_t *gl = (gl_t*)driver.video_data;
      // Reinitialize textures as we might have changed pixel formats.
      gl_reinit_textures(gl, video); 
      return driver.video_data;
   }
#endif

   gl_t *gl = (gl_t*)calloc(1, sizeof(gl_t));
   if (!gl)
      return NULL;

   gl->ctx_driver = gl_get_context(gl);
   if (!gl->ctx_driver)
   {
      free(gl);
      return NULL;
   }

   gl->video_info = *video;

   RARCH_LOG("Found GL context: %s\n", gl->ctx_driver->ident);

   context_get_video_size_func(gl, &gl->full_x, &gl->full_y);
   RARCH_LOG("Detecting screen resolution %ux%u.\n", gl->full_x, gl->full_y);

   context_swap_interval_func(gl, video->vsync ? g_settings.video.swap_interval : 0);

   unsigned win_width  = video->width;
   unsigned win_height = video->height;
   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = gl->full_x;
      win_height = gl->full_y;
   }

   if (!context_set_video_mode_func(gl, win_width, win_height, video->fullscreen))
   {
      free(gl);
      return NULL;
   }

   glGetError(); // Clear out potential error flags incase we use cached context.

   const char *vendor = (const char*)glGetString(GL_VENDOR);
   const char *renderer = (const char*)glGetString(GL_RENDERER);
   RARCH_LOG("[GL]: Vendor: %s, Renderer: %s.\n", vendor, renderer);

   const char *version = (const char*)glGetString(GL_VERSION);
   RARCH_LOG("[GL]: Version: %s.\n", version);

#ifndef RARCH_CONSOLE
   rglgen_resolve_symbols(gl->ctx_driver->get_proc_address);
#endif

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   if (!resolve_extensions(gl))
   {
      context_destroy_func(gl);
      free(gl);
      return NULL;
   }

#ifdef GL_DEBUG
   gl_begin_debug(gl);
#endif

   gl->vsync      = video->vsync;
   gl->fullscreen = video->fullscreen;
   
   // Get real known video size, which might have been altered by context.
   context_get_video_size_func(gl, &gl->win_width, &gl->win_height);
   RARCH_LOG("GL: Using resolution %ux%u\n", gl->win_width, gl->win_height);

   if (gl->full_x || gl->full_y) // We got bogus from gfx_ctx_get_video_size. Replace.
   {
      gl->full_x = gl->win_width;
      gl->full_y = gl->win_height;
   }

   struct retro_hw_render_callback *hw_render = &g_extern.system.hw_render_callback;
   gl->vertex_ptr = hw_render->bottom_left_origin ? vertexes : vertexes_flipped;

   // Better pipelining with GPU due to synchronous glSubTexImage. Multiple async PBOs would be an alternative,
   // but still need multiple textures with PREV.
   gl->textures = 4;
#ifdef HAVE_FBO
#if defined(HAVE_OPENGLES3)
   gl->hw_render_use = hw_render->context_type == RETRO_HW_CONTEXT_OPENGLES2 || hw_render->context_type == RETRO_HW_CONTEXT_OPENGLES3;
#elif defined(HAVE_OPENGLES2)
   gl->hw_render_use = hw_render->context_type == RETRO_HW_CONTEXT_OPENGLES2;
#else
   gl->hw_render_use = hw_render->context_type == RETRO_HW_CONTEXT_OPENGL ||
      g_extern.system.hw_render_callback.context_type == RETRO_HW_CONTEXT_OPENGL_CORE;
#endif
   if (gl->hw_render_use)
      gl->textures = 1; // All on GPU, no need to excessively create textures.
#endif
   gl->white_color_ptr = white_color;

#ifdef HAVE_GLSL
   gl_glsl_set_get_proc_address(gl->ctx_driver->get_proc_address);
   gl_glsl_set_context_type(gl->core_context, hw_render->version_major, hw_render->version_minor);
#endif

   if (!gl_shader_init(gl))
   {
      RARCH_ERR("[GL]: Shader init failed.\n");
      context_destroy_func(gl);
      free(gl);
      return NULL;
   }

   if (gl->shader)
   {
      unsigned minimum = gl->shader->get_prev_textures();
      gl->textures = max(minimum + 1, gl->textures);
   }

   RARCH_LOG("GL: Using %u textures.\n", gl->textures);
   RARCH_LOG("GL: Loaded %u program(s).\n", gl_shader_num(gl));

   gl->tex_w = RARCH_SCALE_BASE * video->input_scale;
   gl->tex_h = RARCH_SCALE_BASE * video->input_scale;

   gl->keep_aspect = video->force_aspect;

   // Apparently need to set viewport for passes when we aren't using FBOs.
   gl_set_shader_viewport(gl, 0);
   gl_set_shader_viewport(gl, 1);

   bool force_smooth = false;
   if (gl_shader_filter_type(gl, 1, &force_smooth))
      gl->tex_filter = force_smooth ? GL_LINEAR : GL_NEAREST;
   else
      gl->tex_filter = video->smooth ? GL_LINEAR : GL_NEAREST;
   gl->wrap_mode = gl_wrap_type_to_enum(gl_shader_wrap_type(gl, 1));

   gl_set_texture_fmts(gl, video->rgb32);

#ifndef HAVE_OPENGLES
   if (!gl->core_context)
      glEnable(GL_TEXTURE_2D);
#endif

   glDisable(GL_DEPTH_TEST);
   glDisable(GL_CULL_FACE);
   glDisable(GL_DITHER);

   memcpy(gl->tex_coords, tex_coords, sizeof(tex_coords));
   gl->coords.vertex         = gl->vertex_ptr;
   gl->coords.tex_coord      = gl->tex_coords;
   gl->coords.color          = gl->white_color_ptr;
   gl->coords.lut_tex_coord  = tex_coords;

   // Empty buffer that we use to clear out the texture with on res change.
   gl->empty_buf = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);

#if !defined(HAVE_PSGL)
   gl->conv_buffer = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);
   if (!gl->conv_buffer)
   {
      context_destroy_func(gl);
      free(gl);
      return NULL;
   }
#endif

   gl_init_textures(gl, video);
   gl_init_textures_data(gl);

#ifdef HAVE_FBO
   // Set up render to texture.
   gl_init_fbo(gl, gl->tex_w, gl->tex_h);

#ifndef HAVE_GCMGL
   if (gl->hw_render_use && !gl_init_hw_render(gl, gl->tex_w, gl->tex_h))
   {
      context_destroy_func(gl);
      free(gl);
      return NULL;
   }
#endif
#endif

   if (input && input_data)
      context_input_driver_func(gl, input, input_data);
   
#ifndef RARCH_CONSOLE
   if (g_settings.video.font_enable)
#endif
   {
      gl->font_ctx = gl_font_init_first(gl, g_settings.video.font_path, g_settings.video.font_size,
            gl->win_width, gl->win_height);
   }

#if !defined(HAVE_OPENGLES) && defined(HAVE_FFMPEG)
   gl_init_pbo_readback(gl);
#endif

   if (!gl_check_error())
   {
      context_destroy_func(gl);
      free(gl);
      return NULL;
   }

   return gl;
}

static bool gl_alive(void *data)
{
   gl_t *gl = (gl_t*)data;
   bool quit = false, resize = false;

   context_check_window_func(gl, &quit,
         &resize, &gl->win_width, &gl->win_height,
         g_extern.frame_count);

   if (quit)
      gl->quitting = true;
   else if (resize)
      gl->should_resize = true;

   return !gl->quitting;
}

static bool gl_focus(void *data)
{
   gl_t *gl = (gl_t*)data;
   return context_has_focus_func(gl);
}

static void gl_update_tex_filter_frame(gl_t *gl)
{
   unsigned i;
   bool smooth = false;
   if (!gl_shader_filter_type(gl, 1, &smooth))
      smooth = g_settings.video.smooth;
   GLenum wrap_mode = gl_wrap_type_to_enum(gl_shader_wrap_type(gl, 1));

   gl->video_info.smooth = smooth;
   GLuint new_filt = smooth ? GL_LINEAR : GL_NEAREST;
   if (new_filt == gl->tex_filter && wrap_mode == gl->wrap_mode)
      return;

   gl->tex_filter = new_filt;
   gl->wrap_mode = wrap_mode;
   for (i = 0; i < gl->textures; i++)
   {
      if (gl->texture[i])
      {
         glBindTexture(GL_TEXTURE_2D, gl->texture[i]);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->wrap_mode);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->wrap_mode);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_filter);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_filter);
      }
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

#if defined(HAVE_GLSL) || defined(HAVE_CG)
static bool gl_set_shader(void *data, enum rarch_shader_type type, const char *path)
{
   gl_t *gl = (gl_t*)data;

   if (type == RARCH_SHADER_NONE)
      return false;

   gl_shader_deinit(gl);

   switch (type)
   {
#ifdef HAVE_GLSL
      case RARCH_SHADER_GLSL:
         gl->shader = &gl_glsl_backend;
         break;
#endif

#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         gl->shader = &gl_cg_backend;
         break;
#endif

      default:
         gl->shader = NULL;
         break;
   }

   if (!gl->shader)
   {
      RARCH_ERR("[GL]: Cannot find shader core for path: %s.\n", path);
      return false;
   }

#ifdef HAVE_FBO
   gl_deinit_fbo(gl);
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
#endif

   if (!gl->shader->init(gl, path))
   {
      RARCH_WARN("[GL]: Failed to set multipass shader. Falling back to stock.\n");
      bool ret = gl->shader->init(gl, NULL);
      if (!ret)
         gl->shader = NULL;
      return false;
   }

   gl_update_tex_filter_frame(gl);

   if (gl->shader)
   {
      unsigned textures = gl->shader->get_prev_textures() + 1;
      if (textures > gl->textures) // Have to reinit a bit.
      {
#if defined(HAVE_FBO) && !defined(HAVE_GCMGL)
         gl_deinit_hw_render(gl);
#endif

         glDeleteTextures(gl->textures, gl->texture);
#if defined(HAVE_PSGL)
         glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
         glDeleteBuffers(1, &gl->pbo);
#endif
         gl->textures = textures;
         RARCH_LOG("GL: Using %u textures.\n", gl->textures);
         gl->tex_index = 0;
         gl_init_textures(gl, &gl->video_info);
         gl_init_textures_data(gl);

#if defined(HAVE_FBO) && !defined(HAVE_GCMGL)
         if (gl->hw_render_use)
            gl_init_hw_render(gl, gl->tex_w, gl->tex_h);
#endif
      }
   }

#ifdef HAVE_FBO
   // Set up render to texture again.
   gl_init_fbo(gl, gl->tex_w, gl->tex_h);
#endif

   // Apparently need to set viewport for passes when we aren't using FBOs.
   gl_set_shader_viewport(gl, 0);
   gl_set_shader_viewport(gl, 1);
   return true;
}
#endif

static void gl_viewport_info(void *data, struct rarch_viewport *vp)
{
   gl_t *gl = (gl_t*)data;
   *vp = gl->vp;
   vp->full_width  = gl->win_width;
   vp->full_height = gl->win_height;

   // Adjust as GL viewport is bottom-up.
   unsigned top_y = vp->y + vp->height;
   unsigned top_dist = gl->win_height - top_y;
   vp->y = top_dist;
}

#ifndef NO_GL_READ_PIXELS
static bool gl_read_viewport(void *data, uint8_t *buffer)
{
   gl_t *gl = (gl_t*)data;
    
   RARCH_PERFORMANCE_INIT(read_viewport);
   RARCH_PERFORMANCE_START(read_viewport);

#if defined(HAVE_FFMPEG) && !defined(HAVE_OPENGLES)
   if (gl->pbo_readback_enable)
   {
      if (!gl->pbo_readback_valid) // We haven't buffered up enough frames yet, come back later.
         return false;

      glBindBuffer(GL_PIXEL_PACK_BUFFER, gl->pbo_readback[gl->pbo_readback_index]);
      const void *ptr = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
      if (!ptr)
      {
         RARCH_ERR("Failed to map pixel unpack buffer.\n");
         return false;
      }

      scaler_ctx_scale(&gl->pbo_readback_scaler, buffer, ptr);
      glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
      glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
   }
   else // Use slow synchronous readbacks. Use this with plain screenshots as we don't really care about performance in this case.
#endif
   {
      // GLES2 only guarantees GL_RGBA/GL_UNSIGNED_BYTE readbacks so do just that ...
      // GLES2 also doesn't support reading back data from front buffer, so render
      // a cached frame and have gl_frame() do the readback while it's in the back buffer.
      // Keep codepath similar for GLES and desktop GL.

      unsigned num_pixels = gl->vp.width * gl->vp.height;

      gl->readback_buffer_screenshot = malloc(num_pixels * sizeof(uint32_t));
      if (!gl->readback_buffer_screenshot)
      {
         RARCH_PERFORMANCE_STOP(read_viewport);
         return false;
      }

      rarch_render_cached_frame();

      uint8_t *dst = buffer;
      const uint8_t *src = (const uint8_t*)gl->readback_buffer_screenshot;
      unsigned i;
      for (i = 0; i < num_pixels; i++, dst += 3, src += 4)
      {
         dst[0] = src[2]; // RGBA -> BGR.
         dst[1] = src[1];
         dst[2] = src[0];
      }

      free(gl->readback_buffer_screenshot);
      gl->readback_buffer_screenshot = NULL;
   }

   RARCH_PERFORMANCE_STOP(read_viewport);
   return true;
}
#endif

#if defined(HAVE_MENU)
static void gl_restart(void)
{
   gl_t *gl = (gl_t*)driver.video_data;

   if (!gl)
	   return;

   void *data = driver.video_data;
   driver.video_data = NULL;
   gl_free(data);
#ifdef HAVE_CG
   gl_cg_invalidate_context();
#endif
   init_video_input();
}
#endif

#ifdef HAVE_OVERLAY
static void gl_free_overlay(gl_t *gl);
static bool gl_overlay_load(void *data, const struct texture_image *images, unsigned num_images)
{
   unsigned i;
   gl_t *gl = (gl_t*)data;

   gl_free_overlay(gl);
   gl->overlay = (struct gl_overlay_data*)calloc(num_images, sizeof(*gl->overlay));
   if (!gl->overlay)
      return false;

   gl->overlays = num_images;

   for (i = 0; i < num_images; i++)
   {
      struct gl_overlay_data *data = &gl->overlay[i];
      glGenTextures(1, &data->tex);
      glBindTexture(GL_TEXTURE_2D, data->tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

      glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(images[i].width * sizeof(uint32_t)));
      glTexImage2D(GL_TEXTURE_2D, 0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32,
            images[i].width, images[i].height, 0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
            RARCH_GL_FORMAT32, images[i].pixels);

      gl_overlay_tex_geom(gl, i, 0, 0, 1, 1); // Default. Stretch to whole screen.
      gl_overlay_vertex_geom(gl, i, 0, 0, 1, 1);
      gl->overlay[i].alpha_mod = 1.0f;
   }

   return true;
}

static void gl_overlay_tex_geom(void *data,
      unsigned image,
      GLfloat x, GLfloat y,
      GLfloat w, GLfloat h)
{
   gl_t *gl = (gl_t*)data;
   struct gl_overlay_data *o = &gl->overlay[image];

   o->tex_coord[0] = x;     o->tex_coord[1] = y;
   o->tex_coord[2] = x + w; o->tex_coord[3] = y;
   o->tex_coord[4] = x;     o->tex_coord[5] = y + h;
   o->tex_coord[6] = x + w; o->tex_coord[7] = y + h;
}

static void gl_overlay_vertex_geom(void *data,
      unsigned image,
      float x, float y,
      float w, float h)
{
   gl_t *gl = (gl_t*)data;
   struct gl_overlay_data *o = &gl->overlay[image];

   // Flipped, so we preserve top-down semantics.
   y = 1.0f - y;
   h = -h;

   o->vertex_coord[0] = x;     o->vertex_coord[1] = y;
   o->vertex_coord[2] = x + w; o->vertex_coord[3] = y;
   o->vertex_coord[4] = x;     o->vertex_coord[5] = y + h;
   o->vertex_coord[6] = x + w; o->vertex_coord[7] = y + h;
}

static void gl_overlay_enable(void *data, bool state)
{
   gl_t *gl = (gl_t*)data;
   gl->overlay_enable = state;
   if (gl->ctx_driver->show_mouse && gl->fullscreen)
      gl->ctx_driver->show_mouse(gl, state);
}

static void gl_overlay_full_screen(void *data, bool enable)
{
   gl_t *gl = (gl_t*)data;
   gl->overlay_full_screen = enable;
}

static void gl_overlay_set_alpha(void *data, unsigned image, float mod)
{
   gl_t *gl = (gl_t*)data;
   gl->overlay[image].alpha_mod = mod;
}

static void gl_render_overlay(void *data)
{
   unsigned i, j;
   gl_t *gl = (gl_t*)data;

   GLfloat white_color_mod[16] = {
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
   };

   glEnable(GL_BLEND);

   if (gl->overlay_full_screen)
      glViewport(0, 0, gl->win_width, gl->win_height);

   for (i = 0; i < gl->overlays; i++)
   {
      // Ensure that we reset the attrib array.
      if (gl->shader)
         gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

      glBindTexture(GL_TEXTURE_2D, gl->overlay[i].tex);
      for (j = 0; j < 4; j++)
         white_color_mod[3 + j * 4] = gl->overlay[i].alpha_mod;
      gl->coords.vertex    = gl->overlay[i].vertex_coord;
      gl->coords.tex_coord = gl->overlay[i].tex_coord;
      gl->coords.color     = white_color_mod;
      gl_shader_set_coords(gl, &gl->coords, &gl->mvp_no_rot);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   }

   glDisable(GL_BLEND);
   gl->coords.vertex    = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color     = gl->white_color_ptr;
   if (gl->overlay_full_screen)
      glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
}

static const video_overlay_interface_t gl_overlay_interface = {
   gl_overlay_enable,
   gl_overlay_load,
   gl_overlay_tex_geom,
   gl_overlay_vertex_geom,
   gl_overlay_full_screen,
   gl_overlay_set_alpha,
};

static void gl_get_overlay_interface(void *data, const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &gl_overlay_interface;
}
#endif

#ifdef HAVE_FBO
static uintptr_t gl_get_current_framebuffer(void *data)
{
   gl_t *gl = (gl_t*)data;
   return gl->hw_render_fbo[(gl->tex_index + 1) % gl->textures];
}

static retro_proc_address_t gl_get_proc_address(void *data, const char *sym)
{
   gl_t *gl = (gl_t*)data;
   return gl->ctx_driver->get_proc_address(sym);
}
#endif

static void gl_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   gl_t *gl = (gl_t*)data;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         gfx_set_square_pixel_viewport(g_extern.system.av_info.geometry.base_width, g_extern.system.av_info.geometry.base_height);
         break;

      case ASPECT_RATIO_CORE:
         gfx_set_core_viewport();
         break;

      case ASPECT_RATIO_CONFIG:
         gfx_set_config_viewport();
         break;

      default:
         break;
   }

   g_extern.system.aspect_ratio = aspectratio_lut[aspect_ratio_idx].value;
   gl->keep_aspect = true;
   gl->should_resize = true;
}

#if defined(HAVE_MENU)
static void gl_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   gl_t *gl = (gl_t*)data;

   if (!gl->rgui_texture)
   {
      glGenTextures(1, &gl->rgui_texture);
      glBindTexture(GL_TEXTURE_2D, gl->rgui_texture);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   }
   else
      glBindTexture(GL_TEXTURE_2D, gl->rgui_texture);

   gl->rgui_texture_alpha = alpha;

   unsigned base_size = rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);
   glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(width * base_size));

   if (rgb32)
   {
      glTexImage2D(GL_TEXTURE_2D,
            0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32,
            width, height,
            0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
            RARCH_GL_FORMAT32, frame);
   }
   else
   {
      glTexImage2D(GL_TEXTURE_2D,
            0, GL_RGBA, width, height, 0, GL_RGBA,
            GL_UNSIGNED_SHORT_4_4_4_4, frame);
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

static void gl_set_texture_enable(void *data, bool state, bool full_screen)
{
   gl_t *gl = (gl_t*)data;
   gl->rgui_texture_enable = state;
   gl->rgui_texture_full_screen = full_screen;
}
#endif

static void gl_apply_state_changes(void *data)
{
   gl_t *gl = (gl_t*)data;
   gl->should_resize = true;
}

static void gl_set_osd_msg(void *data, const char *msg, void *userdata)
{
   gl_t *gl = (gl_t*)data;
   font_params_t *params = (font_params_t*)userdata;

   if (gl->font_ctx)
      gl->font_ctx->render_msg(gl, msg, params);
}

static void gl_show_mouse(void *data, bool state)
{
   gl_t *gl = (gl_t*)data;
   if (gl->ctx_driver->show_mouse)
      gl->ctx_driver->show_mouse(gl, state);
}

static const video_poke_interface_t gl_poke_interface = {
   NULL,
#ifdef HAVE_FBO
   gl_get_current_framebuffer,
   gl_get_proc_address,
#endif
   gl_set_aspect_ratio,
   gl_apply_state_changes,
#if defined(HAVE_MENU)
   gl_set_texture_frame,
   gl_set_texture_enable,
#endif
   gl_set_osd_msg,

   gl_show_mouse,
};

static void gl_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &gl_poke_interface;
}

const video_driver_t video_gl = {
   gl_init,
   gl_frame,
   gl_set_nonblock_state,
   gl_alive,
   gl_focus,

#if defined(HAVE_GLSL) || defined(HAVE_CG)
   gl_set_shader,
#else
   NULL,
#endif

   gl_free,
   "gl",

#if defined(HAVE_MENU)
   gl_restart,
#endif
   gl_set_rotation,

   gl_viewport_info,

#ifndef NO_GL_READ_PIXELS
   gl_read_viewport,
#else
   NULL,
#endif

#ifdef HAVE_OVERLAY
   gl_get_overlay_interface,
#endif
   gl_get_poke_interface,
};


