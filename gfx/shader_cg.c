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

#ifdef _MSC_VER
#pragma comment(lib, "cg")
#pragma comment(lib, "cggl")
#endif

#include "shader_cg.h"
#include "shader_common.h"
#include <Cg/cg.h>
#include <Cg/cgGL.h>
#include "../general.h"
#include <string.h>
#include "../compat/strl.h"
#include "../conf/config_file.h"
#include "image/image.h"
#include "../dynamic.h"
#include "../compat/posix_string.h"
#include "../file.h"

#include "state_tracker.h"

//#define RARCH_CG_DEBUG

// Used when we call deactivate() since just unbinding the program didn't seem to work... :(
static const char *stock_cg_program =
      "struct input"
      "{"
      "  float2 tex_coord;"
      "  float4 color;"
      "  float4 vertex_coord;"
      "  uniform float4x4 mvp_matrix;"
      "  uniform sampler2D texture;"
      "};"
      "struct vertex_data"
      "{"
      "  float2 tex;"
      "  float4 color;"
      "};"
      "void main_vertex"
      "("
      "	out float4 oPosition : POSITION,"
      "  input IN,"
      "  out vertex_data vert"
      ")"
      "{"
      "	oPosition = mul(IN.mvp_matrix, IN.vertex_coord);"
      "  vert = vertex_data(IN.tex_coord, IN.color);"
      "}"
      ""
      "float4 main_fragment(input IN, vertex_data vert, uniform sampler2D s0 : TEXUNIT0) : COLOR"
      "{"
      "  return vert.color * tex2D(s0, vert.tex);"
      "}";

#ifdef RARCH_CG_DEBUG
static void cg_error_handler(CGcontext ctx, CGerror error, void *data)
{
   (void)ctx;
   (void)data;

   switch (error)
   {
      case CG_INVALID_PARAM_HANDLE_ERROR:
         RARCH_ERR("CG: Invalid param handle.\n");
         break;

      case CG_INVALID_PARAMETER_ERROR:
         RARCH_ERR("CG: Invalid parameter.\n");
         break;

      default:
         break;
   }

   RARCH_ERR("CG error: \"%s\"\n", cgGetErrorString(error));
}
#endif

static CGcontext cgCtx;

struct cg_fbo_params
{
   CGparameter vid_size_f;
   CGparameter tex_size_f;
   CGparameter vid_size_v;
   CGparameter tex_size_v;
   CGparameter tex;
   CGparameter coord;
};

#define MAX_LUT_TEXTURES 8
#define MAX_VARIABLES 64
#define PREV_TEXTURES (MAX_TEXTURES - 1)

struct cg_program
{
   CGprogram vprg;
   CGprogram fprg;

   CGparameter tex;
   CGparameter lut_tex;
   CGparameter color;
   CGparameter vertex;

   CGparameter vid_size_f;
   CGparameter tex_size_f;
   CGparameter out_size_f;
   CGparameter frame_cnt_f;
   CGparameter frame_dir_f;
   CGparameter vid_size_v;
   CGparameter tex_size_v;
   CGparameter out_size_v;
   CGparameter frame_cnt_v;
   CGparameter frame_dir_v;
   CGparameter mvp;

   struct cg_fbo_params fbo[GFX_MAX_SHADERS];
   struct cg_fbo_params orig;
   struct cg_fbo_params prev[PREV_TEXTURES];
};

static struct cg_program prg[GFX_MAX_SHADERS];
static const char **cg_arguments;
static bool cg_active;
static CGprofile cgVProf, cgFProf;
static unsigned active_index;

static struct gfx_shader *cg_shader;

static state_tracker_t *state_tracker;
static GLuint lut_textures[MAX_LUT_TEXTURES];

static CGparameter cg_attribs[PREV_TEXTURES + 1 + 4 + GFX_MAX_SHADERS];
static unsigned cg_attrib_index;

static void gl_cg_reset_attrib(void)
{
   unsigned i;
   for (i = 0; i < cg_attrib_index; i++)
      cgGLDisableClientState(cg_attribs[i]);
   cg_attrib_index = 0;
}

static bool gl_cg_set_mvp(void *data, const math_matrix *mat)
{
   (void)data;
   if (cg_active && prg[active_index].mvp)
   {
      cgGLSetMatrixParameterfc(prg[active_index].mvp, mat->data);
      return true;
   }
   else
      return false;
}

#define SET_COORD(name, coords_name, len) do { \
   if (prg[active_index].name) \
   { \
      cgGLSetParameterPointer(prg[active_index].name, len, GL_FLOAT, 0, coords->coords_name); \
      cgGLEnableClientState(prg[active_index].name); \
      cg_attribs[cg_attrib_index++] = prg[active_index].name; \
   } \
} while(0)

static bool gl_cg_set_coords(const struct gl_coords *coords)
{
   if (!cg_active)
      return false;

   SET_COORD(vertex, vertex, 2);
   SET_COORD(tex, tex_coord, 2);
   SET_COORD(lut_tex, lut_tex_coord, 2);
   SET_COORD(color, color, 4);

   return true;
}

#define set_param_2f(param, x, y) \
   if (param) cgGLSetParameter2f(param, x, y)
#define set_param_1f(param, x) \
   if (param) cgGLSetParameter1f(param, x)

static void gl_cg_set_params(void *data, unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height,
      unsigned out_width, unsigned out_height,
      unsigned frame_count,
      const struct gl_tex_info *info,
      const struct gl_tex_info *prev_info,
      const struct gl_tex_info *fbo_info,
      unsigned fbo_info_cnt)
{
   (void)data;
   unsigned i;
   if (!cg_active || (active_index == 0) || (active_index == GL_SHADER_STOCK_BLEND))
      return;

   // Set frame.
   set_param_2f(prg[active_index].vid_size_f, width, height);
   set_param_2f(prg[active_index].tex_size_f, tex_width, tex_height);
   set_param_2f(prg[active_index].out_size_f, out_width, out_height);
   set_param_1f(prg[active_index].frame_dir_f, g_extern.frame_is_reverse ? -1.0 : 1.0);

   set_param_2f(prg[active_index].vid_size_v, width, height);
   set_param_2f(prg[active_index].tex_size_v, tex_width, tex_height);
   set_param_2f(prg[active_index].out_size_v, out_width, out_height);
   set_param_1f(prg[active_index].frame_dir_v, g_extern.frame_is_reverse ? -1.0 : 1.0);

   if (prg[active_index].frame_cnt_f || prg[active_index].frame_cnt_v)
   {
      unsigned modulo = cg_shader->pass[active_index - 1].frame_count_mod;
      if (modulo)
         frame_count %= modulo;

      set_param_1f(prg[active_index].frame_cnt_f, (float)frame_count);
      set_param_1f(prg[active_index].frame_cnt_v, (float)frame_count);
   }

   // Set orig texture.
   CGparameter param = prg[active_index].orig.tex;
   if (param)
   {
      cgGLSetTextureParameter(param, info->tex);
      cgGLEnableTextureParameter(param);
   }

   set_param_2f(prg[active_index].orig.vid_size_v, info->input_size[0], info->input_size[1]);
   set_param_2f(prg[active_index].orig.vid_size_f, info->input_size[0], info->input_size[1]);
   set_param_2f(prg[active_index].orig.tex_size_v, info->tex_size[0],   info->tex_size[1]);
   set_param_2f(prg[active_index].orig.tex_size_f, info->tex_size[0],   info->tex_size[1]);
   if (prg[active_index].orig.coord)
   {
      cgGLSetParameterPointer(prg[active_index].orig.coord, 2, GL_FLOAT, 0, info->coord);
      cgGLEnableClientState(prg[active_index].orig.coord);
      cg_attribs[cg_attrib_index++] = prg[active_index].orig.coord;
   }

   // Set prev textures.
   for (i = 0; i < PREV_TEXTURES; i++)
   {
      param = prg[active_index].prev[i].tex;
      if (param)
      {
         cgGLSetTextureParameter(param, prev_info[i].tex);
         cgGLEnableTextureParameter(param);
      }

      set_param_2f(prg[active_index].prev[i].vid_size_v, prev_info[i].input_size[0], prev_info[i].input_size[1]);
      set_param_2f(prg[active_index].prev[i].vid_size_f, prev_info[i].input_size[0], prev_info[i].input_size[1]);
      set_param_2f(prg[active_index].prev[i].tex_size_v, prev_info[i].tex_size[0],   prev_info[i].tex_size[1]);
      set_param_2f(prg[active_index].prev[i].tex_size_f, prev_info[i].tex_size[0],   prev_info[i].tex_size[1]);

      if (prg[active_index].prev[i].coord)
      {
         cgGLSetParameterPointer(prg[active_index].prev[i].coord, 2, GL_FLOAT, 0, prev_info[i].coord);
         cgGLEnableClientState(prg[active_index].prev[i].coord);
         cg_attribs[cg_attrib_index++] = prg[active_index].prev[i].coord;
      }
   }

   // Set lookup textures.
   for (i = 0; i < cg_shader->luts; i++)
   {
      CGparameter fparam = cgGetNamedParameter(prg[active_index].fprg, cg_shader->lut[i].id);
      if (fparam)
      {
         cgGLSetTextureParameter(fparam, lut_textures[i]);
         cgGLEnableTextureParameter(fparam);
      }

      CGparameter vparam = cgGetNamedParameter(prg[active_index].vprg, cg_shader->lut[i].id);
      if (vparam)
      {
         cgGLSetTextureParameter(vparam, lut_textures[i]);
         cgGLEnableTextureParameter(vparam);
      }
   }

   // Set FBO textures.
   if (active_index > 2)
   {
      for (i = 0; i < fbo_info_cnt; i++)
      {
         if (prg[active_index].fbo[i].tex)
         {
            cgGLSetTextureParameter(prg[active_index].fbo[i].tex, fbo_info[i].tex);
            cgGLEnableTextureParameter(prg[active_index].fbo[i].tex);
         }

         set_param_2f(prg[active_index].fbo[i].vid_size_v, fbo_info[i].input_size[0], fbo_info[i].input_size[1]);
         set_param_2f(prg[active_index].fbo[i].vid_size_f, fbo_info[i].input_size[0], fbo_info[i].input_size[1]);

         set_param_2f(prg[active_index].fbo[i].tex_size_v, fbo_info[i].tex_size[0], fbo_info[i].tex_size[1]);
         set_param_2f(prg[active_index].fbo[i].tex_size_f, fbo_info[i].tex_size[0], fbo_info[i].tex_size[1]);

         if (prg[active_index].fbo[i].coord)
         {
            cgGLSetParameterPointer(prg[active_index].fbo[i].coord, 2, GL_FLOAT, 0, fbo_info[i].coord);
            cgGLEnableClientState(prg[active_index].fbo[i].coord);
            cg_attribs[cg_attrib_index++] = prg[active_index].fbo[i].coord;
         }
      }
   }

   // Set state parameters
   if (state_tracker)
   {
      // Only query uniforms in first pass.
      static struct state_tracker_uniform info[MAX_VARIABLES];
      static unsigned cnt = 0;

      if (active_index == 1)
         cnt = state_get_uniform(state_tracker, info, MAX_VARIABLES, frame_count);

      for (i = 0; i < cnt; i++)
      {
         CGparameter param_v = cgGetNamedParameter(prg[active_index].vprg, info[i].id);
         CGparameter param_f = cgGetNamedParameter(prg[active_index].fprg, info[i].id);
         set_param_1f(param_v, info[i].value);
         set_param_1f(param_f, info[i].value);
      }
   }
}

static void gl_cg_deinit_progs(void)
{
   unsigned i;
   RARCH_LOG("CG: Destroying programs.\n");
   cgGLUnbindProgram(cgFProf);
   cgGLUnbindProgram(cgVProf);

   // Programs may alias [0].
   for (i = 1; i < GFX_MAX_SHADERS; i++)
   {
      if (prg[i].fprg && prg[i].fprg != prg[0].fprg)
         cgDestroyProgram(prg[i].fprg);
      if (prg[i].vprg && prg[i].vprg != prg[0].vprg)
         cgDestroyProgram(prg[i].vprg);
   }

   if (prg[0].fprg)
      cgDestroyProgram(prg[0].fprg);
   if (prg[0].vprg)
      cgDestroyProgram(prg[0].vprg);

   memset(prg, 0, sizeof(prg));
}

static void gl_cg_deinit_state(void)
{
   gl_cg_reset_attrib();
   cg_active = false;

   gl_cg_deinit_progs();

   if (cg_shader && cg_shader->luts)
   {
      glDeleteTextures(cg_shader->luts, lut_textures);
      memset(lut_textures, 0, sizeof(lut_textures));
   }

   if (state_tracker)
   {
      state_tracker_free(state_tracker);
      state_tracker = NULL;
   }

   free(cg_shader);
   cg_shader = NULL;
}

// Final deinit.
static void gl_cg_deinit_context_state(void)
{
   // Destroying context breaks on PS3 for some unknown reason.
#ifndef __CELLOS_LV2__
   if (cgCtx)
   {
      RARCH_LOG("CG: Destroying context.\n");
      cgDestroyContext(cgCtx);
      cgCtx = NULL;
   }
#endif
}

// Full deinit.
static void gl_cg_deinit(void)
{
   gl_cg_deinit_state();
   gl_cg_deinit_context_state();
}

#define SET_LISTING(type) \
{ \
   const char *list = cgGetLastListing(cgCtx); \
   if (list) \
      listing_##type = strdup(list); \
}

static bool load_program(unsigned index, const char *prog, bool path_is_file)
{
   bool ret = true;
   char *listing_f = NULL;
   char *listing_v = NULL;

   if (path_is_file)
   {
      prg[index].fprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE, prog, cgFProf, "main_fragment", cg_arguments);
      SET_LISTING(f);
      prg[index].vprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE, prog, cgVProf, "main_vertex", cg_arguments);
      SET_LISTING(v);
   }
   else
   {
      prg[index].fprg = cgCreateProgram(cgCtx, CG_SOURCE, prog, cgFProf, "main_fragment", cg_arguments);
      SET_LISTING(f);
      prg[index].vprg = cgCreateProgram(cgCtx, CG_SOURCE, prog, cgVProf, "main_vertex", cg_arguments);
      SET_LISTING(v);
   }

   if (!prg[index].fprg || !prg[index].vprg)
   {
      RARCH_ERR("CG error: %s\n", cgGetErrorString(cgGetError()));
      if (listing_f)
         RARCH_ERR("Fragment:\n%s\n", listing_f);
      else if (listing_v)
         RARCH_ERR("Vertex:\n%s\n", listing_v);

      ret = false;
      goto end;
   }

   cgGLLoadProgram(prg[index].fprg);
   cgGLLoadProgram(prg[index].vprg);

end:
   free(listing_f);
   free(listing_v);
   return ret;
}

static void set_program_base_attrib(unsigned i);

static bool load_stock(void)
{
   if (!load_program(0, stock_cg_program, false))
   {
      RARCH_ERR("Failed to compile passthrough shader, is something wrong with your environment?\n");
      return false;
   }

   set_program_base_attrib(0);

   return true;
}

static bool load_plain(const char *path)
{
   if (!load_stock())
      return false;

   cg_shader = (struct gfx_shader*)calloc(1, sizeof(*cg_shader));
   if (!cg_shader)
      return false;

   cg_shader->passes = 1;

   if (path)
   {
      RARCH_LOG("Loading Cg file: %s\n", path);
      strlcpy(cg_shader->pass[0].source.cg, path, sizeof(cg_shader->pass[0].source.cg));
      if (!load_program(1, path, true))
         return false;
   }
   else
   {
      RARCH_LOG("Loading stock Cg file.\n");
      prg[1] = prg[0];
   }

   return true;
}

#define print_buf(buf, ...) snprintf(buf, sizeof(buf), __VA_ARGS__)

static void load_texture_data(GLuint obj, const struct texture_image *img, bool smooth, GLenum wrap)
{
   glBindTexture(GL_TEXTURE_2D, obj);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST);

#ifndef HAVE_PSGL
   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
#endif
   glTexImage2D(GL_TEXTURE_2D,
         0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32, img->width, img->height,
         0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32, RARCH_GL_FORMAT32, img->pixels);
}

static bool load_textures(void)
{
   unsigned i;
   if (!cg_shader->luts)
      return true;

   glGenTextures(cg_shader->luts, lut_textures);

   for (i = 0; i < cg_shader->luts; i++)
   {
      RARCH_LOG("Loading image from: \"%s\".\n",
            cg_shader->lut[i].path);

      struct texture_image img = {0};
      if (!texture_image_load(cg_shader->lut[i].path, &img))
      {
         RARCH_ERR("Failed to load picture ...\n");
         return false;
      }

      load_texture_data(lut_textures[i], &img,
            cg_shader->lut[i].filter != RARCH_FILTER_NEAREST,
            gl_wrap_type_to_enum(cg_shader->lut[i].wrap));
      texture_image_free(&img);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   return true;
}

static bool load_imports(void)
{
   unsigned i;
   if (!cg_shader->variables)
      return true;

   struct state_tracker_info tracker_info = {0};

   for (i = 0; i < cg_shader->variables; i++)
   {
      unsigned memtype;
      switch (cg_shader->variable[i].ram_type)
      {
         case RARCH_STATE_WRAM:
            memtype = RETRO_MEMORY_SYSTEM_RAM;
            break;

         default:
            memtype = -1u;
      }

      if ((memtype != -1u) && (cg_shader->variable[i].addr >= pretro_get_memory_size(memtype)))
      {
         RARCH_ERR("Address out of bounds.\n");
         return false;
      }
   }

   tracker_info.wram = (uint8_t*)pretro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   tracker_info.info = cg_shader->variable;
   tracker_info.info_elem = cg_shader->variables;

#ifdef HAVE_PYTHON
   if (*cg_shader->script_path)
   {
      tracker_info.script = cg_shader->script_path;
      tracker_info.script_is_file = true;
   }

   tracker_info.script_class = *cg_shader->script_class ? cg_shader->script_class : NULL;
#endif

   state_tracker = state_tracker_init(&tracker_info);
   if (!state_tracker)
      RARCH_WARN("Failed to initialize state tracker.\n");

   return true;
}

static bool load_shader(unsigned i)
{
   RARCH_LOG("Loading Cg shader: \"%s\".\n",
         cg_shader->pass[i].source.cg);

   if (!load_program(i + 1, cg_shader->pass[i].source.cg, true))
      return false;

   return true;
}

static bool load_preset(const char *path)
{
   unsigned i;
   if (!load_stock())
      return false;

   RARCH_LOG("Loading Cg meta-shader: %s\n", path);
   config_file_t *conf = config_file_new(path);
   if (!conf)
   {
      RARCH_ERR("Failed to load preset.\n");
      return false;
   }

   if (!cg_shader)
      cg_shader = (struct gfx_shader*)calloc(1, sizeof(*cg_shader));
   if (!cg_shader)
      return false;

   if (!gfx_shader_read_conf_cgp(conf, cg_shader))
   {
      RARCH_ERR("Failed to parse CGP file.\n");
      config_file_free(conf);
      return false;
   }

   config_file_free(conf);
   gfx_shader_resolve_relative(cg_shader, path);

   if (cg_shader->passes > GFX_MAX_SHADERS - 3)
   {
      RARCH_WARN("Too many shaders ... Capping shader amount to %d.\n", GFX_MAX_SHADERS - 3);
      cg_shader->passes = GFX_MAX_SHADERS - 3;
   }
   for (i = 0; i < cg_shader->passes; i++)
   {
      if (!load_shader(i))
      {
         RARCH_ERR("Failed to load shaders ...\n");
         return false;
      }
   }

   if (!load_textures())
   {
      RARCH_ERR("Failed to load lookup textures ...\n");
      return false;
   }

   if (!load_imports())
   {
      RARCH_ERR("Failed to load imports ...\n");
      return false;
   }

   return true;
}

static void set_program_base_attrib(unsigned i)
{
   CGparameter param = cgGetFirstParameter(prg[i].vprg, CG_PROGRAM);
   for (; param; param = cgGetNextParameter(param))
   {
      if (cgGetParameterDirection(param) != CG_IN || cgGetParameterVariability(param) != CG_VARYING)
         continue;

      const char *semantic = cgGetParameterSemantic(param);
      if (!semantic)
         continue;

      RARCH_LOG("CG: Found semantic \"%s\" in prog #%u.\n", semantic, i);

      if (strcmp(semantic, "TEXCOORD") == 0 || strcmp(semantic, "TEXCOORD0") == 0)
         prg[i].tex = param;
      else if (strcmp(semantic, "COLOR") == 0 || strcmp(semantic, "COLOR0") == 0)
         prg[i].color = param;
      else if (strcmp(semantic, "POSITION") == 0)
         prg[i].vertex = param;
      else if (strcmp(semantic, "TEXCOORD1") == 0)
         prg[i].lut_tex = param;
   }

   if (!prg[i].tex)
      prg[i].tex = cgGetNamedParameter(prg[i].vprg, "IN.tex_coord");
   if (!prg[i].color)
      prg[i].color = cgGetNamedParameter(prg[i].vprg, "IN.color");
   if (!prg[i].vertex)
      prg[i].vertex = cgGetNamedParameter(prg[i].vprg, "IN.vertex_coord");
   if (!prg[i].lut_tex)
      prg[i].lut_tex = cgGetNamedParameter(prg[i].vprg, "IN.lut_tex_coord");
}

static void set_pass_attrib(struct cg_program *prg, struct cg_fbo_params *fbo,
      const char *attr)
{
   char attr_buf[64];

   snprintf(attr_buf, sizeof(attr_buf), "%s.texture", attr);
   if (!fbo->tex)
      fbo->tex = cgGetNamedParameter(prg->fprg, attr_buf);

   snprintf(attr_buf, sizeof(attr_buf), "%s.video_size", attr);
   if (!fbo->vid_size_v)
      fbo->vid_size_v = cgGetNamedParameter(prg->vprg, attr_buf);
   if (!fbo->vid_size_f)
      fbo->vid_size_f = cgGetNamedParameter(prg->fprg, attr_buf);

   snprintf(attr_buf, sizeof(attr_buf), "%s.texture_size", attr);
   if (!fbo->tex_size_v)
      fbo->tex_size_v = cgGetNamedParameter(prg->vprg, attr_buf);
   if (!fbo->tex_size_f)
      fbo->tex_size_f = cgGetNamedParameter(prg->fprg, attr_buf);

   snprintf(attr_buf, sizeof(attr_buf), "%s.tex_coord", attr);
   if (!fbo->coord)
      fbo->coord = cgGetNamedParameter(prg->vprg, attr_buf);
}

static void set_program_attributes(unsigned i)
{
   unsigned j;
   cgGLBindProgram(prg[i].fprg);
   cgGLBindProgram(prg[i].vprg);

   set_program_base_attrib(i);

   prg[i].vid_size_f = cgGetNamedParameter(prg[i].fprg, "IN.video_size");
   prg[i].tex_size_f = cgGetNamedParameter(prg[i].fprg, "IN.texture_size");
   prg[i].out_size_f = cgGetNamedParameter(prg[i].fprg, "IN.output_size");
   prg[i].frame_cnt_f = cgGetNamedParameter(prg[i].fprg, "IN.frame_count");
   prg[i].frame_dir_f = cgGetNamedParameter(prg[i].fprg, "IN.frame_direction");
   prg[i].vid_size_v = cgGetNamedParameter(prg[i].vprg, "IN.video_size");
   prg[i].tex_size_v = cgGetNamedParameter(prg[i].vprg, "IN.texture_size");
   prg[i].out_size_v = cgGetNamedParameter(prg[i].vprg, "IN.output_size");
   prg[i].frame_cnt_v = cgGetNamedParameter(prg[i].vprg, "IN.frame_count");
   prg[i].frame_dir_v = cgGetNamedParameter(prg[i].vprg, "IN.frame_direction");

   prg[i].mvp = cgGetNamedParameter(prg[i].vprg, "modelViewProj");
   if (!prg[i].mvp)
      prg[i].mvp = cgGetNamedParameter(prg[i].vprg, "IN.mvp_matrix");

   prg[i].orig.tex = cgGetNamedParameter(prg[i].fprg, "ORIG.texture");
   prg[i].orig.vid_size_v = cgGetNamedParameter(prg[i].vprg, "ORIG.video_size");
   prg[i].orig.vid_size_f = cgGetNamedParameter(prg[i].fprg, "ORIG.video_size");
   prg[i].orig.tex_size_v = cgGetNamedParameter(prg[i].vprg, "ORIG.texture_size");
   prg[i].orig.tex_size_f = cgGetNamedParameter(prg[i].fprg, "ORIG.texture_size");
   prg[i].orig.coord = cgGetNamedParameter(prg[i].vprg, "ORIG.tex_coord");

   if (i > 1)
   {
      char pass_str[64];
      snprintf(pass_str, sizeof(pass_str), "PASSPREV%u", i);
      set_pass_attrib(&prg[i], &prg[i].orig, pass_str);
   }

   for (j = 0; j < PREV_TEXTURES; j++)
   {
      char attr_buf_tex[64];
      char attr_buf_vid_size[64];
      char attr_buf_tex_size[64];
      char attr_buf_coord[64];
      static const char *prev_names[PREV_TEXTURES] = {
         "PREV",
         "PREV1",
         "PREV2",
         "PREV3",
         "PREV4",
         "PREV5",
         "PREV6",
      };

      snprintf(attr_buf_tex,      sizeof(attr_buf_tex),      "%s.texture", prev_names[j]);
      snprintf(attr_buf_vid_size, sizeof(attr_buf_vid_size), "%s.video_size", prev_names[j]);
      snprintf(attr_buf_tex_size, sizeof(attr_buf_tex_size), "%s.texture_size", prev_names[j]);
      snprintf(attr_buf_coord,    sizeof(attr_buf_coord),    "%s.tex_coord", prev_names[j]);

      prg[i].prev[j].tex = cgGetNamedParameter(prg[i].fprg, attr_buf_tex);

      prg[i].prev[j].vid_size_v = cgGetNamedParameter(prg[i].vprg, attr_buf_vid_size);
      prg[i].prev[j].vid_size_f = cgGetNamedParameter(prg[i].fprg, attr_buf_vid_size);

      prg[i].prev[j].tex_size_v = cgGetNamedParameter(prg[i].vprg, attr_buf_tex_size);
      prg[i].prev[j].tex_size_f = cgGetNamedParameter(prg[i].fprg, attr_buf_tex_size);

      prg[i].prev[j].coord = cgGetNamedParameter(prg[i].vprg, attr_buf_coord);
   }

   for (j = 0; j < i - 1; j++)
   {
      char pass_str[64];
      snprintf(pass_str, sizeof(pass_str), "PASS%u", j + 1);
      set_pass_attrib(&prg[i], &prg[i].fbo[j], pass_str);
      snprintf(pass_str, sizeof(pass_str), "PASSPREV%u", i - j); 
      set_pass_attrib(&prg[i], &prg[i].fbo[j], pass_str);
   }
}

static bool gl_cg_init(void *data, const char *path)
{
   unsigned i;
   (void)data;
#ifdef HAVE_CG_RUNTIME_COMPILER
   cgRTCgcInit();
#endif

   if (!cgCtx)
      cgCtx = cgCreateContext();

   if (cgCtx == NULL)
   {
      RARCH_ERR("Failed to create Cg context\n");
      return false;
   }

#ifdef RARCH_CG_DEBUG
   cgGLSetDebugMode(CG_TRUE);
   cgSetErrorHandler(cg_error_handler, NULL);
#endif

   cgFProf = cgGLGetLatestProfile(CG_GL_FRAGMENT);
   cgVProf = cgGLGetLatestProfile(CG_GL_VERTEX);
   if (cgFProf == CG_PROFILE_UNKNOWN || cgVProf == CG_PROFILE_UNKNOWN)
   {
      RARCH_ERR("Invalid profile type\n");
      goto error;
   }
#ifndef HAVE_GCMGL
   RARCH_LOG("[Cg]: Vertex profile: %s\n", cgGetProfileString(cgVProf));
   RARCH_LOG("[Cg]: Fragment profile: %s\n", cgGetProfileString(cgFProf));
#endif
   cgGLSetOptimalOptions(cgFProf);
   cgGLSetOptimalOptions(cgVProf);
   cgGLEnableProfile(cgFProf);
   cgGLEnableProfile(cgVProf);

   if (path && strcmp(path_get_extension(path), "cgp") == 0)
   {
      if (!load_preset(path))
         goto error;
   }
   else
   {
      if (!load_plain(path))
         goto error;
   }

   prg[0].mvp = cgGetNamedParameter(prg[0].vprg, "IN.mvp_matrix");
   for (i = 1; i <= cg_shader->passes; i++)
      set_program_attributes(i);

   // If we aren't using last pass non-FBO shader, 
   // this shader will be assumed to be "fixed-function".
   // Just use prg[0] for that pass, which will be
   // pass-through.
   prg[cg_shader->passes + 1] = prg[0]; 

   // No need to apply Android hack in Cg.
   prg[GL_SHADER_STOCK_BLEND] = prg[0];

   cgGLBindProgram(prg[1].fprg);
   cgGLBindProgram(prg[1].vprg);

   cg_active = true;
   return true;

error:
   gl_cg_deinit();
   return false;
}

static void gl_cg_use(void *data, unsigned index)
{
   (void)data;

   if (cg_active && prg[index].vprg && prg[index].fprg)
   {
      gl_cg_reset_attrib();

      active_index = index;
      cgGLBindProgram(prg[index].vprg);
      cgGLBindProgram(prg[index].fprg);
   }
}

static unsigned gl_cg_num(void)
{
   if (cg_active)
      return cg_shader->passes;
   else
      return 0;
}

static bool gl_cg_filter_type(unsigned index, bool *smooth)
{
   if (cg_active && index)
   {
      if (cg_shader->pass[index - 1].filter == RARCH_FILTER_UNSPEC)
         return false;
      *smooth = cg_shader->pass[index - 1].filter == RARCH_FILTER_LINEAR;
      return true;
   }
   else
      return false;
}

static enum gfx_wrap_type gl_cg_wrap_type(unsigned index)
{
   if (cg_active && index)
      return cg_shader->pass[index - 1].wrap;
   else
      return RARCH_WRAP_BORDER;
}

static void gl_cg_shader_scale(unsigned index, struct gfx_fbo_scale *scale)
{
   if (cg_active && index)
      *scale = cg_shader->pass[index - 1].fbo;
   else
      scale->valid = false;
}

static unsigned gl_cg_get_prev_textures(void)
{
   unsigned i, j;
   if (!cg_active)
      return 0;

   unsigned max_prev = 0;
   for (i = 1; i <= cg_shader->passes; i++)
      for (j = 0; j < PREV_TEXTURES; j++)
         if (prg[i].prev[j].tex)
            max_prev = max(j + 1, max_prev);

   return max_prev;
}

void gl_cg_set_compiler_args(const char **argv)
{
   cg_arguments = argv;
}

void gl_cg_invalidate_context(void)
{
   cgCtx = NULL;
}

const gl_shader_backend_t gl_cg_backend = {
   gl_cg_init,
   gl_cg_deinit,
   gl_cg_set_params,
   gl_cg_use,
   gl_cg_num,
   gl_cg_filter_type,
   gl_cg_wrap_type,
   gl_cg_shader_scale,
   gl_cg_set_coords,
   gl_cg_set_mvp,
   gl_cg_get_prev_textures,

   RARCH_SHADER_CG,
};

