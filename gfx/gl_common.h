/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __GL_COMMON_H
#define __GL_COMMON_H

#include "../general.h"
#include "fonts/fonts.h"
#include "math/matrix.h"
#include "gfx_context.h"
#include "scaler/scaler.h"
#include "fonts/gl_font.h"
#include "shader_parse.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <string.h>

#ifdef HAVE_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#include "glsym/glsym.h"

#define context_get_video_size_func(gl, win, height)     gl->ctx_driver->get_video_size(gl, win, height)
#define context_update_window_title_func(gl)             gl->ctx_driver->update_window_title(gl)
#define context_destroy_func(gl)                         gl->ctx_driver->destroy(gl)
#define context_translate_aspect_func(gl, width, height) gl->ctx_driver->translate_aspect(gl, width, height)
#define context_set_resize_func(gl, width, height)       gl->ctx_driver->set_resize(gl, width, height)
#define context_swap_buffers_func(gl)                    gl->ctx_driver->swap_buffers(gl)
#define context_swap_interval_func(gl, var)              gl->ctx_driver->swap_interval(gl, var)
#define context_has_focus_func(gl)                       gl->ctx_driver->has_focus(gl)
#define context_check_window_func(gl, quit, resize, width, height, frame_count) \
   gl->ctx_driver->check_window(gl, quit, resize, width, height, frame_count)

#define context_set_video_mode_func(gl, width, height, fullscreen) gl->ctx_driver->set_video_mode(gl, width, height, fullscreen)
#define context_input_driver_func(gl, input, input_data) gl->ctx_driver->input_driver(gl, input, input_data)

#ifdef HAVE_EGL
#define context_init_egl_image_buffer_func(gl, video)    gl->ctx_driver->init_egl_image_buffer(gl, video)
#define context_write_egl_image_func(gl, frame, width, height, pitch, base_size, tex_index, img) \
   gl->ctx_driver->write_egl_image(gl, frame, width, height, pitch, base_size, tex_index,img)
#endif

static inline bool gl_check_error(void)
{
   int error = glGetError();
   switch (error)
   {
      case GL_INVALID_ENUM:
         RARCH_ERR("GL: Invalid enum.\n");
         break;
      case GL_INVALID_VALUE:
         RARCH_ERR("GL: Invalid value.\n");
         break;
      case GL_INVALID_OPERATION:
         RARCH_ERR("GL: Invalid operation.\n");
         break;
      case GL_OUT_OF_MEMORY:
         RARCH_ERR("GL: Out of memory.\n");
         break;
      case GL_NO_ERROR:
         return true;
      default:
         RARCH_ERR("Non specified GL error.\n");
   }

   return false;
}

static inline unsigned get_alignment(unsigned pitch)
{
   if (pitch & 1)
      return 1;
   if (pitch & 2)
      return 2;
   if (pitch & 4)
      return 4;
   return 8;
}

struct gl_fbo_rect
{
   unsigned img_width;
   unsigned img_height;
   unsigned max_img_width;
   unsigned max_img_height;
   unsigned width;
   unsigned height;
};

struct gl_ortho
{
   GLfloat left;
   GLfloat right;
   GLfloat bottom;
   GLfloat top;
   GLfloat znear;
   GLfloat zfar;
};

struct gl_tex_info
{
   GLuint tex;
   GLfloat input_size[2];
   GLfloat tex_size[2];
   GLfloat coord[8];
};

struct gl_coords
{
   const GLfloat *vertex;
   const GLfloat *color;
   const GLfloat *tex_coord;
   const GLfloat *lut_tex_coord;
};

typedef struct gl_shader_backend gl_shader_backend_t;

#define MAX_SHADERS 16
#define MAX_TEXTURES 8

struct gl_overlay_data
{
   GLuint tex;
   GLfloat tex_coord[8];
   GLfloat vertex_coord[8];
   GLfloat alpha_mod;
};

typedef struct gl
{
   const gfx_ctx_driver_t *ctx_driver;
   const gl_shader_backend_t *shader;

   bool vsync;
   GLuint texture[MAX_TEXTURES];
   unsigned tex_index; // For use with PREV.
   unsigned textures;
   struct gl_tex_info prev_info[MAX_TEXTURES];
   GLuint tex_filter;

   void *empty_buf;

   void *conv_buffer;
   struct scaler_ctx scaler;

   unsigned frame_count;

#ifdef HAVE_FBO
   // Render-to-texture, multipass shaders
   GLuint fbo[MAX_SHADERS];
   GLuint fbo_texture[MAX_SHADERS];
   struct gl_fbo_rect fbo_rect[MAX_SHADERS];
   struct gfx_fbo_scale fbo_scale[MAX_SHADERS];
   int fbo_pass;
   bool fbo_inited;

   GLuint hw_render_fbo[MAX_TEXTURES];
   GLuint hw_render_depth[MAX_TEXTURES];
   bool hw_render_fbo_init;
   bool hw_render_depth_init;
   bool has_fp_fbo;
#endif
   bool hw_render_use;

   bool should_resize;
   bool quitting;
   bool fullscreen;
   bool keep_aspect;
   unsigned rotation;

   unsigned full_x, full_y;

   unsigned win_width;
   unsigned win_height;
   struct rarch_viewport vp;
   unsigned vp_out_width;
   unsigned vp_out_height;
   unsigned last_width[MAX_TEXTURES];
   unsigned last_height[MAX_TEXTURES];
   unsigned tex_w, tex_h;
   GLfloat tex_coords[8];
   math_matrix mvp, mvp_no_rot;

   struct gl_coords coords;
   const GLfloat *vertex_ptr;
   const GLfloat *white_color_ptr;

   GLuint pbo;

   GLenum internal_fmt;
   GLenum texture_type; // RGB565 or ARGB
   GLenum texture_fmt;
   GLenum wrap_mode;
   unsigned base_size; // 2 or 4
#ifdef HAVE_OPENGLES
   bool support_unpack_row_length;
#else
   bool have_es2_compat;
#endif

   // Fonts
   void *font;
   const gl_font_renderer_t *font_ctx;
   const font_renderer_driver_t *font_driver;
   GLuint font_tex;
   GLint max_font_size;
   int font_tex_w, font_tex_h;
   uint32_t *font_tex_buf;
   char font_last_msg[256];
   int font_last_width, font_last_height;
   GLfloat font_color[16];
   GLfloat font_color_dark[16];

   bool egl_images;
   video_info_t video_info;

#ifdef HAVE_OVERLAY
   struct gl_overlay_data *overlay;
   unsigned overlays;
   bool overlay_enable;
   bool overlay_full_screen;
#endif

#if !defined(HAVE_OPENGLES) && defined(HAVE_FFMPEG)
   // PBOs used for asynchronous viewport readbacks.
   GLuint pbo_readback[4];
   bool pbo_readback_enable;
   bool pbo_readback_valid;
   unsigned pbo_readback_index;
   struct scaler_ctx pbo_readback_scaler;
#endif
   void *readback_buffer_screenshot;

#if defined(HAVE_MENU)
   GLuint rgui_texture;
   bool rgui_texture_enable;
   bool rgui_texture_full_screen;
   GLfloat rgui_texture_alpha;
#endif

#ifdef HAVE_GL_SYNC
#define MAX_FENCES 4
   bool have_sync;
   GLsync fences[MAX_FENCES];
   unsigned fence_count;
#endif

   bool core_context;
   GLuint vao;
} gl_t;

#if defined(HAVE_PSGL)
#define RARCH_GL_INTERNAL_FORMAT32 GL_ARGB_SCE
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGB5 // TODO: Verify if this is really 565 or just 555.
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA
#define RARCH_GL_TEXTURE_TYPE16 GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_RGB5
#elif defined(HAVE_OPENGLES)
// Imgtec/SGX headers have this missing.
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif
#ifdef IOS
#define RARCH_GL_INTERNAL_FORMAT32 GL_RGBA // Stupid Apple
#else
#define RARCH_GL_INTERNAL_FORMAT32 GL_BGRA_EXT
#endif
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGB
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA_EXT
#define RARCH_GL_TEXTURE_TYPE16 GL_RGB
#define RARCH_GL_FORMAT32 GL_UNSIGNED_BYTE
#define RARCH_GL_FORMAT16 GL_UNSIGNED_SHORT_5_6_5
#else
// On desktop, we always use 32-bit.
#define RARCH_GL_INTERNAL_FORMAT32 GL_RGBA8
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGBA8
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA
#define RARCH_GL_TEXTURE_TYPE16 GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_UNSIGNED_INT_8_8_8_8_REV

// GL_RGB565 internal format isn't in desktop GL until 4.1 core (ARB_ES2_compatibility).
// Check for this.
#ifndef GL_RGB565
#define GL_RGB565 0x8D62
#endif
#define RARCH_GL_INTERNAL_FORMAT16_565 GL_RGB565
#define RARCH_GL_TEXTURE_TYPE16_565 GL_RGB
#define RARCH_GL_FORMAT16_565 GL_UNSIGNED_SHORT_5_6_5
#endif

// Platform specific workarounds/hacks.
#if defined(__CELLOS_LV2__)
#define NO_GL_READ_PIXELS

// Performance hacks
#ifdef HAVE_GCMGL
extern GLvoid* glMapBufferTextureReferenceRA( GLenum target, GLenum access );
extern GLboolean glUnmapBufferTextureReferenceRA( GLenum target );
extern void glBufferSubDataTextureReferenceRA( GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data );
#define glMapBuffer(target, access) glMapBufferTextureReferenceRA(target, access)
#define glUnmapBuffer(target) glUnmapBufferTextureReferenceRA(target)
#define glBufferSubData(target, offset, size, data) glBufferSubDataTextureReferenceRA(target, offset, size, data)
#endif
#endif

#if defined(HAVE_OPENGL_MODERN) || defined(HAVE_OPENGLES2) || defined(HAVE_PSGL)
#define NO_GL_FF_VERTEX
#endif

#if defined(HAVE_OPENGL_MODERN) || defined(HAVE_OPENGLES2) || defined(HAVE_PSGL)
#define NO_GL_FF_MATRIX
#endif

#if defined(HAVE_OPENGLES2) // TODO: Figure out exactly what.
#define NO_GL_CLAMP_TO_BORDER
#endif

#if defined(HAVE_OPENGLES)

#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH  0x0CF2
#endif

#endif

void gl_set_projection(void *data, struct gl_ortho *ortho, bool allow_rotate);
void gl_set_viewport(void *data, unsigned width, unsigned height, bool force_full, bool allow_rotate);
void gl_shader_set_coords(void *data, const struct gl_coords *coords, const math_matrix *mat);

void gl_init_fbo(void *data, unsigned width, unsigned height);
void gl_deinit_fbo(void *data);

static inline GLenum gl_wrap_type_to_enum(enum gfx_wrap_type type)
{
   switch (type)
   {
#ifndef HAVE_OPENGLES
      case RARCH_WRAP_BORDER:
         return GL_CLAMP_TO_BORDER;
#else
      case RARCH_WRAP_BORDER:
#endif
      case RARCH_WRAP_EDGE:
         return GL_CLAMP_TO_EDGE;
      case RARCH_WRAP_REPEAT:
         return GL_REPEAT;
      case RARCH_WRAP_MIRRORED_REPEAT:
         return GL_MIRRORED_REPEAT;
      default:
         return 0;
   }
}

#endif

