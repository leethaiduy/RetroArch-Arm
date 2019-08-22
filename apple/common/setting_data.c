/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#include "setting_data.h"
#include "../../config.def.h"

// HACK
struct settings fake_settings;
struct global fake_extern;

void setting_data_load_current()
{
   memcpy(&fake_settings, &g_settings, sizeof(struct settings));
   memcpy(&fake_extern, &g_extern, sizeof(struct global));
}

#define ENFORCE_RANGE(setting, type)                  \
{                                                     \
   if (setting->flags & SD_FLAG_HAS_RANGE)            \
   {                                                  \
      if (*setting->value.type < setting->min)        \
         *setting->value.type = setting->min;         \
      if (*setting->value.type > setting->max)        \
         *setting->value.type = setting->max;         \
   }                                                  \
}

// Input
static const char* get_input_config_prefix(const rarch_setting_t* setting)
{
   static char buffer[32];
   snprintf(buffer, 32, "input%cplayer%d", setting->index ? '_' : '\0', setting->index);
   return buffer;
}

static const char* get_input_config_key(const rarch_setting_t* setting, const char* type)
{
   static char buffer[64];
   snprintf(buffer, 64, "%s_%s%c%s", get_input_config_prefix(setting), setting->name, type ? '_' : '\0', type);
   return buffer;
}

static const char* get_key_name(const rarch_setting_t* setting)
{
   if (BINDFOR(*setting).key == RETROK_UNKNOWN)
      return "nul";

   uint32_t hidkey = input_translate_rk_to_keysym(BINDFOR(*setting).key);
   
   for (int i = 0; apple_key_name_map[i].hid_id; i ++)
      if (apple_key_name_map[i].hid_id == hidkey)
         return apple_key_name_map[i].keyname;
   
   return "nul";
}


static const char* get_button_name(const rarch_setting_t* setting)
{
   static char buffer[32];

   if (BINDFOR(*setting).joykey == NO_BTN)
      return "nul";

   snprintf(buffer, 32, "%lld", BINDFOR(*setting).joykey);
   return buffer;
}

static const char* get_axis_name(const rarch_setting_t* setting)
{
   static char buffer[32];
   
   uint32_t joyaxis = BINDFOR(*setting).joyaxis;
   
   if (AXIS_NEG_GET(joyaxis) != AXIS_DIR_NONE)
      snprintf(buffer, 8, "-%d", AXIS_NEG_GET(joyaxis));
   else if (AXIS_POS_GET(joyaxis) != AXIS_DIR_NONE)
      snprintf(buffer, 8, "+%d", AXIS_POS_GET(joyaxis));
   else
      return "nul";
   
   return buffer;
}

//
void setting_data_reset_setting(const rarch_setting_t* setting)
{
   switch (setting->type)
   {
      case ST_BOOL:   *setting->value.boolean          = setting->default_value.boolean;          break;
      case ST_INT:    *setting->value.integer          = setting->default_value.integer;          break;
      case ST_UINT:   *setting->value.unsigned_integer = setting->default_value.unsigned_integer; break;
      case ST_FLOAT:  *setting->value.fraction         = setting->default_value.fraction;         break;
      case ST_BIND:   *setting->value.keybind          = *setting->default_value.keybind;         break;
      
      case ST_STRING:
      case ST_PATH:
      {  
         if (setting->default_value.string)
         {
            if (setting->type == ST_STRING)
               strlcpy(setting->value.string, setting->default_value.string, setting->size);
            else
               fill_pathname_expand_special(setting->value.string, setting->default_value.string, setting->size);
         }
         break;
      }
      
      default: break;
   }
   
   if (setting->change_handler)
      setting->change_handler(setting);
}

void setting_data_reset(const rarch_setting_t* settings)
{
   memset(&fake_settings, 0, sizeof(fake_settings));
   memset(&fake_extern, 0, sizeof(fake_extern));
   
   for (const rarch_setting_t* i = settings; i->type != ST_NONE; i ++)
      setting_data_reset_setting(i);
}

bool setting_data_load_config_path(const rarch_setting_t* settings, const char* path)
{
   config_file_t* config = config_file_new(path);
   
   if (config)
   {
      setting_data_load_config(settings, config);
      config_file_free(config);
   }
   
   return config;
}

bool setting_data_load_config(const rarch_setting_t* settings, config_file_t* config)
{
   if (!config)
      return false;

   for (const rarch_setting_t* i = settings; i->type != ST_NONE; i ++)
   {
      switch (i->type)
      {
         case ST_BOOL:   config_get_bool  (config, i->name, i->value.boolean); break;            
         case ST_PATH:   config_get_path  (config, i->name, i->value.string, i->size); break;
         case ST_STRING: config_get_array (config, i->name, i->value.string, i->size); break;
         
         case ST_INT:
            config_get_int(config, i->name, i->value.integer);
            ENFORCE_RANGE(i, integer);
            break;

         case ST_UINT:
            config_get_uint(config, i->name, i->value.unsigned_integer);
            ENFORCE_RANGE(i, unsigned_integer);
            break;

         case ST_FLOAT:
            config_get_float(config, i->name, i->value.fraction);
            ENFORCE_RANGE(i, fraction);
            break;         
         
         case ST_BIND:
         {
            const char* prefix = get_input_config_prefix(i);
            input_config_parse_key       (config, prefix, i->name, i->value.keybind);
            input_config_parse_joy_button(config, prefix, i->name, i->value.keybind);
            input_config_parse_joy_axis  (config, prefix, i->name, i->value.keybind);
            break;
         }
         
         case ST_HEX:    break;
         default:        break;
      }
   }
   
   return true;
}

bool setting_data_save_config_path(const rarch_setting_t* settings, const char* path)
{
   config_file_t* config = config_file_new(path);
   
   if (!config)
      config = config_file_new(0);
   
   setting_data_save_config(settings, config);
   bool result = config_file_write(config, path);
   config_file_free(config);
   
   return result;
}

bool setting_data_save_config(const rarch_setting_t* settings, config_file_t* config)
{
   if (!config)
      return false;

   for (const rarch_setting_t* i = settings; i->type != ST_NONE; i ++)
   {
      switch (i->type)
      {
         case ST_BOOL:   config_set_bool  (config, i->name, *i->value.boolean); break;
         case ST_PATH:   config_set_path(config, i->name,  i->value.string); break;
         case ST_STRING: config_set_string(config, i->name,  i->value.string); break;
         
         case ST_INT:
            ENFORCE_RANGE(i, integer);         
            config_set_int(config, i->name, *i->value.integer);
            break;

         case ST_UINT:
            ENFORCE_RANGE(i, unsigned_integer);         
            config_set_uint64(config, i->name, *i->value.unsigned_integer);
            break;

         case ST_FLOAT:
            ENFORCE_RANGE(i, fraction);         
            config_set_float(config, i->name, *i->value.fraction);
            break;

         case ST_BIND:
         {
            config_set_string(config, get_input_config_key(i, 0     ), get_key_name(i));
            config_set_string(config, get_input_config_key(i, "btn" ), get_button_name(i));
            config_set_string(config, get_input_config_key(i, "axis"), get_axis_name(i));
            break;
         }
         
         case ST_HEX:    break;
         default:        break;
      }
   }
   
   return true;
}

const rarch_setting_t* setting_data_find_setting(const rarch_setting_t* settings, const char* name)
{
   if (!name)
      return 0;

   for (const rarch_setting_t* i = settings; i->type != ST_NONE; i ++)
      if (i->type <= ST_GROUP && strcmp(i->name, name) == 0)
         return i;

   return 0;
}

void setting_data_set_with_string_representation(const rarch_setting_t* setting, const char* value)
{
   if (!setting || !value)
      return;
   
   switch (setting->type)
   {
      case ST_INT:
         sscanf(value, "%d", setting->value.integer);
         ENFORCE_RANGE(setting, integer);
         break;
      case ST_UINT:
         sscanf(value, "%u", setting->value.unsigned_integer);
         ENFORCE_RANGE(setting, unsigned_integer);
         break;      
      case ST_FLOAT:
         sscanf(value, "%f", setting->value.fraction);
         ENFORCE_RANGE(setting, fraction);         
         break;

      case ST_PATH:   strlcpy(setting->value.string, value, setting->size); break;
      case ST_STRING: strlcpy(setting->value.string, value, setting->size); break;
      
      default: return;
   }
   
   if (setting->change_handler)
      setting->change_handler(setting);
}

const char* setting_data_get_string_representation(const rarch_setting_t* setting, char* buffer, size_t length)
{
   if (!setting || !buffer || !length)
      return "";

   switch (setting->type)
   {
      case ST_BOOL:   snprintf(buffer, length, "%s", *setting->value.boolean ? "True" : "False"); break;
      case ST_INT:    snprintf(buffer, length, "%d", *setting->value.integer); break;
      case ST_UINT:   snprintf(buffer, length, "%u", *setting->value.unsigned_integer); break;
      case ST_FLOAT:  snprintf(buffer, length, "%f", *setting->value.fraction); break;
      case ST_PATH:   strlcpy(buffer, setting->value.string, length); break;
      case ST_STRING: strlcpy(buffer, setting->value.string, length); break;

      case ST_BIND:
      {
         snprintf(buffer, length, "[KB:%s] [JS:%s] [AX:%s]", get_key_name(setting), get_button_name(setting), get_axis_name(setting));
         break;
      }

      default: return "";
   }

   return buffer;
}

rarch_setting_t setting_data_group_setting(enum setting_type type, const char* name)
{
   rarch_setting_t result = { type, name };
   return result;
}

#define DEFINE_BASIC_SETTING_TYPE(TAG, TYPE, VALUE, SETTING_TYPE) \
rarch_setting_t setting_data_##TAG##_setting(const char* name, const char* description, TYPE* target, TYPE default_value) \
{ \
   rarch_setting_t result = { SETTING_TYPE, name, sizeof(TYPE), description }; \
   result.value.VALUE = target; \
   result.default_value.VALUE = default_value; \
   return result; \
}

DEFINE_BASIC_SETTING_TYPE(bool, bool, boolean, ST_BOOL)
DEFINE_BASIC_SETTING_TYPE(int, int, integer, ST_INT)
DEFINE_BASIC_SETTING_TYPE(uint, unsigned int, unsigned_integer, ST_UINT)
DEFINE_BASIC_SETTING_TYPE(float, float, fraction, ST_FLOAT)

rarch_setting_t setting_data_string_setting(enum setting_type type, const char* name, const char* description, char* target, unsigned size, const char* default_value)
{
   rarch_setting_t result = { type, name, size, description };
   result.value.string = target;
   result.default_value.string = default_value;
   return result;
}

rarch_setting_t setting_data_bind_setting(const char* name, const char* description, struct retro_keybind* target, uint32_t index,
                                    const struct retro_keybind* default_value)
{
   rarch_setting_t result = { ST_BIND, name, 0, description };
   result.value.keybind = target;
   result.default_value.keybind = default_value;
   result.index = index;
   return result;
}


#ifdef IOS
static const uint32_t features = SD_FEATURE_SHADERS;
#elif defined(OSX)
static const uint32_t features = SD_FEATURE_VIDEO_MODE | SD_FEATURE_SHADERS |
                                 SD_FEATURE_VSYNC | SD_FEATURE_AUDIO_DEVICE;
#endif

#define g_settings fake_settings
#define g_extern fake_extern


#define DEFAULT_ME_YO 0
#define NEXT (list[index++])
#define WITH_FEATURE(FTS)                       if (!FTS || features & FTS)
#define START_GROUP(NAME)                       { NEXT = setting_data_group_setting (ST_GROUP, NAME);
#define END_GROUP()                             NEXT = setting_data_group_setting (ST_END_GROUP, 0); }
#define START_SUB_GROUP(NAME)                   { NEXT = setting_data_group_setting (ST_SUB_GROUP, NAME);
#define END_SUB_GROUP()                         NEXT = setting_data_group_setting (ST_END_SUB_GROUP, 0); }
#define CONFIG_BOOL(TARGET, NAME, SHORT, DEF)   NEXT = setting_data_bool_setting  (NAME, SHORT, &TARGET, DEF);
#define CONFIG_INT(TARGET, NAME, SHORT, DEF)    NEXT = setting_data_int_setting   (NAME, SHORT, &TARGET, DEF);
#define CONFIG_UINT(TARGET, NAME, SHORT, DEF)   NEXT = setting_data_uint_setting  (NAME, SHORT, &TARGET, DEF);
#define CONFIG_FLOAT(TARGET, NAME, SHORT, DEF)  NEXT = setting_data_float_setting (NAME, SHORT, &TARGET, DEF);
#define CONFIG_PATH(TARGET, NAME, SHORT, DEF)   NEXT = setting_data_string_setting(ST_PATH, NAME, SHORT, TARGET, sizeof(TARGET), DEF);
#define CONFIG_STRING(TARGET, NAME, SHORT, DEF) NEXT = setting_data_string_setting(ST_STRING, NAME, SHORT, TARGET, sizeof(TARGET), DEF);
#define CONFIG_HEX(TARGET, NAME, SHORT)
#define CONFIG_BIND(TARGET, PLAYER, NAME, SHORT, DEF) \
   NEXT = setting_data_bind_setting  (NAME, SHORT, &TARGET, PLAYER, DEF);

#define WITH_FLAGS(FLAGS) (list[index - 1]).flags |= FLAGS;

#define WITH_RANGE(MIN, MAX)    \
   (list[index - 1]).min = MIN; \
   (list[index - 1]).max = MAX; \
   WITH_FLAGS(SD_FLAG_HAS_RANGE)

#define WITH_VALUES(VALUES) (list[index -1]).values = VALUES;

const rarch_setting_t* setting_data_get_list()
{
   static rarch_setting_t list[512] = { 0 };

   if (list[0].type == ST_NONE)
   {
      unsigned index = 0;

   /***********/
   /* DRIVERS */
   /***********/
   WITH_FEATURE(SD_FEATURE_MULTI_DRIVER) START_GROUP("Drivers")
      START_SUB_GROUP("Drivers")
         CONFIG_STRING(g_settings.video.driver,             "video_driver",               "Video Driver",               config_get_default_video())
         CONFIG_STRING(g_settings.video.gl_context,         "video_gl_context",           "OpenGL Driver",              "")
         CONFIG_STRING(g_settings.audio.driver,             "audio_driver",               "Audio Driver",               config_get_default_audio())
         CONFIG_STRING(g_settings.input.driver,             "input_driver",               "Input Driver",               config_get_default_input())
         CONFIG_STRING(g_settings.input.joypad_driver,      "input_joypad_driver",        "Joypad Driver",              "")
         CONFIG_STRING(g_settings.input.keyboard_layout,    "input_keyboard_layout",      "Keyboard Layout",            DEFAULT_ME_YO)

         #ifdef HAVE_CAMERA
         CONFIG_STRING(g_settings.camera.device,            "camera_device",              "Camera Driver",              config_get_default_camera())
         #endif         
      END_SUB_GROUP()
   END_GROUP()

   /*********/
   /* PATHS */
   /*********/
   START_GROUP("Paths")
      START_SUB_GROUP("Paths")
         CONFIG_PATH(g_settings.libretro,                   "libretro_path",              "libretro Path",              DEFAULT_ME_YO)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         CONFIG_PATH(g_settings.libretro_info_path,         "libretro_info_path",         "Info Path",                  default_libretro_info_path)   WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
         CONFIG_PATH(g_settings.core_options_path,          "core_options_path",          "Core Options Path",          DEFAULT_ME_YO)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         CONFIG_PATH(g_settings.screenshot_directory,       "screenshot_directory",       "Screenshot Directory",       DEFAULT_ME_YO)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
         CONFIG_PATH(g_settings.cheat_database,             "cheat_database_path",        "Cheat Database",             DEFAULT_ME_YO)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         CONFIG_PATH(g_settings.cheat_settings_path,        "cheat_settings_path",        "Cheat Settings",             DEFAULT_ME_YO)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         CONFIG_PATH(g_settings.game_history_path,          "game_history_path",          "Content History Path",       DEFAULT_ME_YO)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         CONFIG_UINT(g_settings.game_history_size,          "game_history_size",          "Content History Size",       game_history_size)

         #ifdef HAVE_RGUI
         CONFIG_PATH(g_settings.rgui_content_directory,     "rgui_browser_directory",     "Content Directory",          DEFAULT_ME_YO)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
         CONFIG_PATH(g_settings.rgui_config_directory,      "rgui_config_directory",      "Config Directory",           DEFAULT_ME_YO)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
         CONFIG_BOOL(g_settings.rgui_show_start_screen,     "rgui_show_start_screen",     "Show Start Screen",          rgui_show_start_screen)
         #endif

         #ifdef HAVE_OVERLAY
         CONFIG_PATH(g_extern.overlay_dir,                  "overlay_directory",          "Overlay Directory",          default_overlay_dir) WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
         #endif
         
         // savefile_directory
         // savestate_directory
         // system_directory
      END_SUB_GROUP()
   END_GROUP()


   /*************/
   /* EMULATION */
   /*************/
   START_GROUP("Emulation")
      START_SUB_GROUP("Emulation")
         CONFIG_BOOL(g_settings.pause_nonactive,            "pause_nonactive",            "Pause when inactive",        pause_nonactive)
         CONFIG_BOOL(g_settings.rewind_enable,              "rewind_enable",              "Enable Rewind",              rewind_enable)
//       CONFIG_INT(g_settings.rewind_buffer_size,          "rewind_buffer_size",         "Rewind Buffer Size",         rewind_buffer_size)     WITH_SCALE(1000000)
         CONFIG_UINT(g_settings.rewind_granularity,         "rewind_granularity",         "Rewind Granularity",         rewind_granularity)
         CONFIG_FLOAT(g_settings.slowmotion_ratio,          "slowmotion_ratio",           "Slow Motion Ratio",          slowmotion_ratio)       WITH_RANGE(0, 1)
         CONFIG_FLOAT(g_settings.fastforward_ratio,         "fastforward_ratio",          "Fast Forward Ratio",         fastforward_ratio)
         CONFIG_BOOL(g_settings.fps_show,                   "fps_show",                   "Show Frame Rate",            DEFAULT_ME_YO)
      END_SUB_GROUP()

      START_SUB_GROUP("Saves")
         CONFIG_UINT(g_settings.autosave_interval,          "autosave_interval",          "Autosave Interval",          autosave_interval)
         CONFIG_BOOL(g_settings.block_sram_overwrite,       "block_sram_overwrite",       "Block SRAM overwrite",       block_sram_overwrite)
         CONFIG_BOOL(g_settings.savestate_auto_index,       "savestate_auto_index",       "Save State Auto Index",      savestate_auto_index)
         CONFIG_BOOL(g_settings.savestate_auto_save,        "savestate_auto_save",        "Auto Save State",            savestate_auto_save)
         CONFIG_BOOL(g_settings.savestate_auto_load,        "savestate_auto_load",        "Auto Load State",            savestate_auto_load)
         CONFIG_UINT(g_extern.state_slot,                   "state_slot",                 "State Slot",                 0)
      END_SUB_GROUP()
   END_GROUP()

   /*********/
   /* VIDEO */
   /*********/
   START_GROUP("Video")
      WITH_FEATURE(SD_FEATURE_VIDEO_MODE) START_SUB_GROUP("Monitor")
         CONFIG_UINT(g_settings.video.monitor_index,        "video_monitor_index",        "Monitor Index",              monitor_index)
         CONFIG_BOOL(g_settings.video.fullscreen,           "video_fullscreen",           "Use Fullscreen mode",        fullscreen)
         CONFIG_BOOL(g_settings.video.windowed_fullscreen,  "video_windowed_fullscreen",  "Windowed Fullscreen Mode",   windowed_fullscreen)
         CONFIG_UINT(g_settings.video.fullscreen_x,         "video_fullscreen_x",         "Fullscreen Width",           fullscreen_x)
         CONFIG_UINT(g_settings.video.fullscreen_y,         "video_fullscreen_y",         "Fullscreen Height",          fullscreen_y)
         CONFIG_FLOAT(g_settings.video.refresh_rate,        "video_refresh_rate",         "Refresh Rate",               refresh_rate)
      END_SUB_GROUP()

      /* Video: Window Manager */
      WITH_FEATURE(SD_FEATURE_WINDOW_MANAGER) START_SUB_GROUP("Window Manager")
         CONFIG_BOOL(g_settings.video.disable_composition,  "video_disable_composition",  "Disable WM Composition",     disable_composition)
      END_SUB_GROUP()

      START_SUB_GROUP("Aspect")
         CONFIG_BOOL(g_settings.video.force_aspect,         "video_force_aspect",         "Force aspect ratio",         force_aspect)
         CONFIG_FLOAT(g_settings.video.aspect_ratio,        "video_aspect_ratio",         "Aspect Ratio",               aspect_ratio)
         CONFIG_BOOL(g_settings.video.aspect_ratio_auto,    "video_aspect_ratio_auto",    "Use Auto Aspect Ratio",      aspect_ratio_auto)
         CONFIG_UINT(g_settings.video.aspect_ratio_idx,     "aspect_ratio_index",         "Aspect Ratio Index",         aspect_ratio_idx)
      END_SUB_GROUP()

      START_SUB_GROUP("Scaling")
         CONFIG_FLOAT(g_settings.video.xscale,              "video_xscale",               "X Scale",                    xscale)
         CONFIG_FLOAT(g_settings.video.yscale,              "video_yscale",               "Y Scale",                    yscale)
         CONFIG_BOOL(g_settings.video.scale_integer,        "video_scale_integer",        "Integer Scale",      scale_integer)

         CONFIG_INT(g_extern.console.screen.viewports.custom_vp.x,         "custom_viewport_x",       "Custom Viewport X",       0)
         CONFIG_INT(g_extern.console.screen.viewports.custom_vp.y,         "custom_viewport_y",       "Custom Viewport Y",       0)
         CONFIG_UINT(g_extern.console.screen.viewports.custom_vp.width,    "custom_viewport_width",   "Custom Viewport Width",   0)
         CONFIG_UINT(g_extern.console.screen.viewports.custom_vp.height,   "custom_viewport_height",  "Custom Viewport Height",  0)

         CONFIG_BOOL(g_settings.video.smooth,               "video_smooth",               "Use bilinear filtering",     video_smooth)
         CONFIG_UINT(g_settings.video.rotation,             "video_rotation",             "Rotation",                   DEFAULT_ME_YO)
      END_SUB_GROUP()

      WITH_FEATURE(SD_FEATURE_SHADERS) START_SUB_GROUP("Shader")
         CONFIG_BOOL(g_settings.video.shader_enable,        "video_shader_enable",        "Enable Shaders",             shader_enable)
         CONFIG_PATH(g_settings.video.shader_dir,           "video_shader_dir",           "Shader Directory",           default_shader_dir)  WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
         CONFIG_PATH(g_settings.video.shader_path,          "video_shader",               "Shader",                     DEFAULT_ME_YO)       WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
      END_SUB_GROUP()

      WITH_FEATURE(SD_FEATURE_VSYNC) START_SUB_GROUP("Sync")
         CONFIG_BOOL(g_settings.video.threaded,             "video_threaded",             "Use threaded video",         video_threaded)
         CONFIG_BOOL(g_settings.video.vsync,                "video_vsync",                "VSync",                      vsync)
         CONFIG_UINT(g_settings.video.swap_interval,        "video_swap_interval",        "VSync Swap interval",        swap_interval)       WITH_RANGE(1, 4)
         CONFIG_BOOL(g_settings.video.hard_sync,            "video_hard_sync",            "Hard GPU Sync",              hard_sync)
         CONFIG_UINT(g_settings.video.hard_sync_frames,     "video_hard_sync_frames",     "Hard GPU Sync Frames",       hard_sync_frames)    WITH_RANGE(0, 3)
         CONFIG_BOOL(g_settings.video.black_frame_insertion,"video_black_frame_insertion","Black Frame Insertion",      black_frame_insertion)
      END_SUB_GROUP()

      START_SUB_GROUP("Misc")
         CONFIG_BOOL(g_settings.video.post_filter_record,   "video_post_filter_record",   "Post filter record",         post_filter_record)
         CONFIG_BOOL(g_settings.video.gpu_record,           "video_gpu_record",           "GPU Record",                 gpu_record)
         CONFIG_BOOL(g_settings.video.gpu_screenshot,       "video_gpu_screenshot",       "GPU Screenshot",             gpu_screenshot)
         CONFIG_BOOL(g_settings.video.allow_rotate,         "video_allow_rotate",         "Allow rotation",             allow_rotate)
         CONFIG_BOOL(g_settings.video.crop_overscan,        "video_crop_overscan",        "Crop Overscan (reload)",     crop_overscan)

         #ifdef HAVE_DYLIB
         CONFIG_PATH(g_settings.video.filter_path,          "video_filter",               "Software filter",            DEFAULT_ME_YO)       WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         #endif
      END_SUB_GROUP()

      START_SUB_GROUP("Messages")
         CONFIG_PATH(g_settings.video.font_path,            "video_font_path",            "Font Path",                  DEFAULT_ME_YO)       WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
         CONFIG_FLOAT(g_settings.video.font_size,           "video_font_size",            "Font Size",                  font_size)
         CONFIG_BOOL(g_settings.video.font_enable,          "video_font_enable",          "Font Enable",                font_enable)
         CONFIG_BOOL(g_settings.video.font_scale,           "video_font_scale",           "Font Scale",                 font_scale)
         CONFIG_FLOAT(g_settings.video.msg_pos_x,           "video_message_pos_x",        "Message X Position",         message_pos_offset_x)
         CONFIG_FLOAT(g_settings.video.msg_pos_y,           "video_message_pos_y",        "Message Y Position",         message_pos_offset_y)
         /* message color */
      END_SUB_GROUP()
   END_GROUP()

   /*********/
   /* AUDIO */
   /*********/
   START_GROUP("Audio")
      START_SUB_GROUP("State")
         CONFIG_BOOL(g_settings.audio.enable,               "audio_enable",               "Enable",                     audio_enable)
         CONFIG_FLOAT(g_settings.audio.volume,              "audio_volume",               "Volume",                     audio_volume)
         CONFIG_BOOL(g_extern.audio_data.mute,              "audio_mute",                 "Mute",                       DEFAULT_ME_YO)
      END_SUB_GROUP()

      START_SUB_GROUP("Sync")
         CONFIG_BOOL(g_settings.audio.sync,                 "audio_sync",                 "Enable Sync",                audio_sync)
         CONFIG_UINT(g_settings.audio.latency,              "audio_latency",              "Latency",                    out_latency)
         CONFIG_BOOL(g_settings.audio.rate_control,         "audio_rate_control",         "Enable Rate Control",        rate_control)
         CONFIG_FLOAT(g_settings.audio.rate_control_delta,  "audio_rate_control_delta",   "Rate Control Delta",         rate_control_delta)
         CONFIG_UINT(g_settings.audio.block_frames,         "audio_block_frames",         "Block Frames",               DEFAULT_ME_YO)
      END_SUB_GROUP()

      WITH_FEATURE(SD_FEATURE_AUDIO_DEVICE) START_SUB_GROUP("Misc")
         CONFIG_STRING(g_settings.audio.device,             "audio_device",               "Device",                     DEFAULT_ME_YO)
         CONFIG_UINT(g_settings.audio.out_rate,             "audio_out_rate",             "Ouput Rate",                 out_rate)
         CONFIG_PATH(g_settings.audio.dsp_plugin,           "audio_dsp_plugin",           "DSP Plugin",                 DEFAULT_ME_YO)          WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
      END_SUB_GROUP()
   END_GROUP()

   /*********/
   /* INPUT */
   /*********/
   START_GROUP("Input")
      START_SUB_GROUP("Input")
         CONFIG_BOOL(g_settings.input.autodetect_enable,    "input_autodetect_enable",    "Use joypad autodetection",   input_autodetect_enable)
         CONFIG_PATH(g_settings.input.autoconfig_dir,       "joypad_autoconfig_dir",      "Joypad Autoconfig Directory",DEFAULT_ME_YO)          WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      END_SUB_GROUP()

      START_SUB_GROUP("Joypad Mapping")
         //TODO: input_libretro_device_p%u
         CONFIG_INT(g_settings.input.joypad_map[0],         "input_player1_joypad_index", "Player 1 Pad Index",         0)
         CONFIG_INT(g_settings.input.joypad_map[1],         "input_player2_joypad_index", "Player 2 Pad Index",         1)
         CONFIG_INT(g_settings.input.joypad_map[2],         "input_player3_joypad_index", "Player 3 Pad Index",         2)
         CONFIG_INT(g_settings.input.joypad_map[3],         "input_player4_joypad_index", "Player 4 Pad Index",         3)
         CONFIG_INT(g_settings.input.joypad_map[4],         "input_player5_joypad_index", "Player 5 Pad Index",         4)
      END_SUB_GROUP()

      START_SUB_GROUP("Turbo/Deadzone")
         CONFIG_FLOAT(g_settings.input.axis_threshold,      "input_axis_threshold",       "Axis Deadzone",              axis_threshold)
         CONFIG_UINT(g_settings.input.turbo_period,         "input_turbo_period",         "Turbo Period",               turbo_period)
         CONFIG_UINT(g_settings.input.turbo_duty_cycle,     "input_duty_cycle",           "Duty Cycle",                 turbo_duty_cycle)
      END_SUB_GROUP()

      START_SUB_GROUP("Misc")
         CONFIG_BOOL(g_settings.input.netplay_client_swap_input, "netplay_client_swap_input", "Swap Netplay Input",     netplay_client_swap_input)
         CONFIG_BOOL(g_settings.input.debug_enable,         "input_debug_enable",         "Enable Input Debugging",     input_debug_enable)
      END_SUB_GROUP()

      #ifdef HAVE_OVERLAY
      START_SUB_GROUP("Overlay")
         CONFIG_PATH(g_settings.input.overlay,              "input_overlay",              "Input Overlay",              DEFAULT_ME_YO) WITH_FLAGS(SD_FLAG_ALLOW_EMPTY) WITH_VALUES("cfg")
         CONFIG_FLOAT(g_settings.input.overlay_opacity,     "input_overlay_opacity",      "Overlay Opacity",            0.7f) WITH_RANGE(0, 1)
         CONFIG_FLOAT(g_settings.input.overlay_scale,       "input_overlay_scale",        "Overlay Scale",              1.0f)
      END_SUB_GROUP()
      #endif

      #ifdef ANDROID
      START_SUB_GROUP("Android")
         CONFIG_INT(g_settings.input.back_behavior,         "input_back_behavior",        "Back Behavior",              BACK_BUTTON_QUIT)
         CONFIG_INT(g_settings.input.icade_profile[0],      "input_autodetect_icade_profile_pad1", "iCade 1",           DEFAULT_ME_YO)
         CONFIG_INT(g_settings.input.icade_profile[1],      "input_autodetect_icade_profile_pad2", "iCade 2",           DEFAULT_ME_YO)
         CONFIG_INT(g_settings.input.icade_profile[2],      "input_autodetect_icade_profile_pad3", "iCade 3",           DEFAULT_ME_YO)
         CONFIG_INT(g_settings.input.icade_profile[3],      "input_autodetect_icade_profile_pad4", "iCade 4",           DEFAULT_ME_YO)
      END_SUB_GROUP()
      #endif

      // The second argument to config bind is 1 based for players and 0 only for meta keys
      START_SUB_GROUP("Meta Keys")
         for (int i = 0; i != RARCH_BIND_LIST_END; i ++)
            if (input_config_bind_map[i].meta)
            {
               const struct input_bind_map* bind = &input_config_bind_map[i];
               CONFIG_BIND(g_settings.input.binds[0][i], 0, bind->base, bind->desc, &retro_keybinds_1[i])
            }
      END_SUB_GROUP()

      for (int player = 0; player < MAX_PLAYERS; player ++)
      {
         const struct retro_keybind* const defaults = (player == 0) ? retro_keybinds_1 : retro_keybinds_rest;
      
         char buffer[32];
         snprintf(buffer, 32, "Player %d", player + 1);
         START_SUB_GROUP(strdup(buffer))
            for (int i = 0; i != RARCH_BIND_LIST_END; i ++)
               if (!input_config_bind_map[i].meta)
               {
                  const struct input_bind_map* bind = &input_config_bind_map[i];
                  CONFIG_BIND(g_settings.input.binds[player][i], player + 1, bind->base, bind->desc, &defaults[i])
               }
         END_SUB_GROUP()
      }
   END_GROUP()

   /********/
   /* Misc */
   /********/
   START_GROUP("Misc")
      START_SUB_GROUP("Misc")
         CONFIG_BOOL(g_extern.config_save_on_exit,          "config_save_on_exit",        "Save Config On Exit",        config_save_on_exit)
         CONFIG_BOOL(g_settings.network_cmd_enable,         "network_cmd_enable",         "Network Commands",           network_cmd_enable)
         //CONFIG_INT(g_settings.network_cmd_port,            "network_cmd_port",           "Network Command Port",       network_cmd_port)
         CONFIG_BOOL(g_settings.stdin_cmd_enable,           "stdin_cmd_enable",           "stdin command",              stdin_cmd_enable)
      END_SUB_GROUP()
   END_GROUP()
   }
   
   return list;
}
