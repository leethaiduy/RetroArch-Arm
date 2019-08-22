/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#ifndef __APPLE_RARCH_SETTING_DATA_H__
#define __APPLE_RARCH_SETTING_DATA_H__

#include "general.h"

enum setting_type { ST_NONE, ST_BOOL, ST_INT, ST_FLOAT, ST_PATH, ST_STRING, ST_HEX, ST_GROUP, ST_SUB_GROUP,
                    ST_END_GROUP, ST_END_SUB_GROUP };

typedef struct
{
   enum setting_type type;

   const char* name;
   
   void* value;
   
   const char* short_description;
   const char* long_description;

   const char** values;

   uint32_t input_player;
   double min;
   double max;
   bool allow_blank;
}  rarch_setting_t;

#define START_GROUP(NAME)                  { ST_GROUP,         NAME, 0,       0,     0, 0, 0, 0.0, 0.0, false },
#define END_GROUP()                        { ST_END_GROUP,     0,    0,       0,     0, 0, 0, 0.0, 0.0, false },
#define START_SUB_GROUP(NAME)              { ST_SUB_GROUP,     NAME, 0,       0,     0, 0, 0, 0.0, 0.0, false },
#define END_SUB_GROUP()                    { ST_END_SUB_GROUP, 0,    0,       0,     0, 0, 0, 0.0, 0.0, false },
#define START_GROUP(NAME)                  { ST_GROUP,         NAME, 0,       0,     0, 0, 0, 0.0, 0.0, false },
#define END_GROUP()                        { ST_END_GROUP,     0,    0,       0,     0, 0, 0, 0.0, 0.0, false },
#define CONFIG_BOOL(TARGET, NAME, SHORT)   { ST_BOOL,          NAME, &TARGET, SHORT, 0, 0, 0, 0.0, 0.0, false },
#define CONFIG_INT(TARGET, NAME, SHORT)    { ST_INT,           NAME, &TARGET, SHORT, 0, 0, 0, 0.0, 0.0, false },
#define CONFIG_FLOAT(TARGET, NAME, SHORT)  { ST_FLOAT,         NAME, &TARGET, SHORT, 0, 0, 0, 0.0, 0.0, false },
#define CONFIG_PATH(TARGET, NAME, SHORT)   { ST_PATH,          NAME, &TARGET, SHORT, 0, 0, 0, 0.0, 0.0, false },
#define CONFIG_STRING(TARGET, NAME, SHORT) { ST_STRING,        NAME, &TARGET, SHORT, 0, 0, 0, 0.0, 0.0, false },
#define CONFIG_HEX(TARGET, NAME, SHORT)    { ST_HEX,           NAME, &TARGET, SHORT, 0, 0, 0, 0.0, 0.0, false },

const rarch_setting_t setting_data[] = 
{
   /***********/
   /* DRIVERS */
   /***********/
   START_GROUP("Drivers")
      START_SUB_GROUP("Drivers")
         CONFIG_STRING(g_settings.video.driver, "video_driver", "Video Driver")
         CONFIG_STRING(g_settings.video.gl_context, "video_gl_context", "OpenGL Driver")
         CONFIG_STRING(g_settings.audio.driver, "audio_driver", "Audio Driver")
         CONFIG_STRING(g_settings.input.driver, "input_driver", "Input Driver")
         CONFIG_STRING(g_settings.input.joypad_driver, "input_joypad_driver", "Joypad Driver")
      END_SUB_GROUP()
   END_GROUP()

   /*********/
   /* PATHS */
   /*********/
   START_GROUP("Paths")
      START_SUB_GROUP("Paths")
         CONFIG_PATH(g_settings.libretro, "libretro_path", "libretro Path")
         CONFIG_PATH(g_settings.core_options_path, "core_options_path", "Core Options Path")
         CONFIG_PATH(g_settings.screenshot_directory, "screenshot_directory", "Screenshot Directory")
         CONFIG_PATH(g_settings.cheat_database, "cheat_database_path", "Cheat Database")
         CONFIG_PATH(g_settings.cheat_settings_path, "cheat_settings_path", "Cheat Settings")
         CONFIG_PATH(g_settings.game_history_path, "game_history_path", "Game History Path")
         CONFIG_INT(g_settings.game_history_size, "game_history_size", "Game History Size")

         #ifdef HAVE_RGUI
            CONFIG_PATH(g_settings.rgui_browser_directory, "rgui_browser_directory", "Browser Directory")
         #endif

         #ifdef HAVE_OVERLAY
            CONFIG_PATH(g_extern.overlay_dir, "overlay_directory", "Overlay Directory")
         #endif
      END_SUB_GROUP()
   END_GROUP()



   /*************/
   /* EMULATION */
   /*************/
   START_GROUP("Emulation")
      START_SUB_GROUP("Emulation")
         CONFIG_BOOL(g_settings.pause_nonactive, "pause_nonactive", "Pause when inactive")
         CONFIG_BOOL(g_settings.rewind_enable, "rewind_enable", "Enable Rewind")
         CONFIG_INT(g_settings.rewind_buffer_size, "rewind_buffer_size", "Rewind Buffer Size") /* *= 1000000 */
         CONFIG_INT(g_settings.rewind_granularity, "rewind_granularity", "Rewind Granularity")
         CONFIG_FLOAT(g_settings.slowmotion_ratio, "slowmotion_ratio", "Slow motion ratio") /* >= 1.0f */

         /* Saves */
         CONFIG_INT(g_settings.autosave_interval, "autosave_interval", "Autosave Interval")
         CONFIG_BOOL(g_settings.block_sram_overwrite, "block_sram_overwrite", "Block SRAM overwrite")
         CONFIG_BOOL(g_settings.savestate_auto_index, "savestate_auto_index", "Save State Auto Index")
         CONFIG_BOOL(g_settings.savestate_auto_save, "savestate_auto_save", "Auto Save State")
         CONFIG_BOOL(g_settings.savestate_auto_load, "savestate_auto_load", "Auto Load State")
      END_SUB_GROUP()
   END_GROUP()

   /*********/
   /* AUDIO */
   /*********/
   START_GROUP("Audio")
      START_SUB_GROUP("Audio")
         CONFIG_BOOL(g_settings.audio.enable, "audio_enable", "Enable")
         CONFIG_FLOAT(g_settings.audio.volume, "audio_volume", "Volume")

         /* Audio: Sync */
         CONFIG_BOOL(g_settings.audio.sync, "audio_sync", "Enable Sync")
         CONFIG_INT(g_settings.audio.latency, "audio_latency", "Latency")
         CONFIG_BOOL(g_settings.audio.rate_control, "audio_rate_control", "Enable Rate Control")
         CONFIG_FLOAT(g_settings.audio.rate_control_delta, "audio_rate_control_delta", "Rate Control Delta")

         /* Audio: Other */
         CONFIG_STRING(g_settings.audio.device, "audio_device", "Device")
         CONFIG_INT(g_settings.audio.out_rate, "audio_out_rate", "Ouput Rate")
         CONFIG_PATH(g_settings.audio.dsp_plugin, "audio_dsp_plugin", "DSP Plugin")
      END_SUB_GROUP()
   END_GROUP()

   /*********/
   /* INPUT */
   /*********/
   START_GROUP("Input")
      START_SUB_GROUP("Input")
         /* Input: Autoconfig */
         CONFIG_BOOL(g_settings.input.autodetect_enable, "input_autodetect_enable", "Use joypad autodetection")
         CONFIG_PATH(g_settings.input.autoconfig_dir, "joypad_autoconfig_dir", "Joypad Autoconfig Directory")

         /* Input: Joypad mapping */
         CONFIG_INT(g_settings.input.joypad_map[0], "input_player1_joypad_index", "Player 1 Pad Index")
         CONFIG_INT(g_settings.input.joypad_map[1], "input_player2_joypad_index", "Player 2 Pad Index")
         CONFIG_INT(g_settings.input.joypad_map[2], "input_player3_joypad_index", "Player 3 Pad Index")
         CONFIG_INT(g_settings.input.joypad_map[3], "input_player4_joypad_index", "Player 4 Pad Index")
         CONFIG_INT(g_settings.input.joypad_map[4], "input_player5_joypad_index", "Player 5 Pad Index")

         /* Input: Turbo/Axis options */
         CONFIG_FLOAT(g_settings.input.axis_threshold, "input_axis_threshold", "Axis Deadzone")
         CONFIG_INT(g_settings.input.turbo_period, "input_turbo_period", "Turbo Period")
         CONFIG_INT(g_settings.input.turbo_duty_cycle, "input_duty_cycle", "Duty Cycle")

         /* Input: Misc */
         CONFIG_BOOL(g_settings.input.netplay_client_swap_input, "netplay_client_swap_input", "Swap Netplay Input")
         CONFIG_BOOL(g_settings.input.debug_enable, "input_debug_enable", "Enable Input Debugging")

         /* Input: Overlay */
         #ifdef HAVE_OVERLAY
            CONFIG_PATH(g_settings.input.overlay, "input_overlay", "Input Overlay")
            CONFIG_FLOAT(g_settings.input.overlay_opacity, "overlay_opacity", "Overlay Opacity")
            CONFIG_FLOAT(g_settings.input.overlay_scale, "overlay_scale", "Overlay Scale")
         #endif

         /* Input: Android */
         #ifdef ANDROID
            CONFIG_INT(g_settings.input.back_behavior, "input_back_behavior", "Back Behavior")
            CONFIG_INT(g_settings.input.icade_profile[0], "input_autodetect_icade_profile_pad1", "iCade 1")
            CONFIG_INT(g_settings.input.icade_profile[1], "input_autodetect_icade_profile_pad2", "iCade 2")
            CONFIG_INT(g_settings.input.icade_profile[2], "input_autodetect_icade_profile_pad3", "iCade 3")
            CONFIG_INT(g_settings.input.icade_profile[3], "input_autodetect_icade_profile_pad4", "iCade 4")
         #endif
      END_SUB_GROUP()
   END_GROUP()

   /*********/
   /* VIDEO */
   /*********/
   START_GROUP("Video")
      START_SUB_GROUP("Monitor")
         CONFIG_INT(g_settings.video.monitor_index, "video_monitor_index", "Monitor Index")
         CONFIG_BOOL(g_settings.video.fullscreen, "video_fullscreen", "Use Fullscreen mode") // if (!g_extern.force_fullscreen)
         CONFIG_BOOL(g_settings.video.windowed_fullscreen, "video_windowed_fullscreen", "Windowed Fullscreen Mode")
         CONFIG_INT(g_settings.video.fullscreen_x, "video_fullscreen_x", "Fullscreen Width")
         CONFIG_INT(g_settings.video.fullscreen_y, "video_fullscreen_y", "Fullscreen Height")
         CONFIG_FLOAT(g_settings.video.refresh_rate, "video_refresh_rate", "Refresh Rate")
      END_SUB_GROUP()
   
      /* Video: Window Manager */
      START_SUB_GROUP("Window Manager")
         CONFIG_BOOL(g_settings.video.disable_composition, "video_disable_composition", "Disable WM Composition")
      END_SUB_GROUP()

      START_SUB_GROUP("Aspect")
         CONFIG_BOOL(g_settings.video.force_aspect, "video_force_aspect", "Force aspect ratio")
         CONFIG_FLOAT(g_settings.video.aspect_ratio, "video_aspect_ratio", "Aspect Ratio")
         CONFIG_INT(g_settings.video.aspect_ratio_idx, "aspect_ratio_index", "Aspect Ratio Index")
         CONFIG_BOOL(g_settings.video.aspect_ratio_auto, "video_aspect_ratio_auto", "Use Auto Aspect Ratio")
      END_SUB_GROUP()

      START_SUB_GROUP("Scaling")
         CONFIG_FLOAT(g_settings.video.xscale, "video_xscale", "X Scale")
         CONFIG_FLOAT(g_settings.video.yscale, "video_yscale", "Y Scale")
         CONFIG_BOOL(g_settings.video.scale_integer, "video_scale_integer", "Force integer scaling")

         CONFIG_INT(g_extern.console.screen.viewports.custom_vp.x, "custom_viewport_x", "Custom Viewport X")
         CONFIG_INT(g_extern.console.screen.viewports.custom_vp.y, "custom_viewport_y", "Custom Viewport Y")
         CONFIG_INT(g_extern.console.screen.viewports.custom_vp.width, "custom_viewport_width", "Custom Viewport Width")
         CONFIG_INT(g_extern.console.screen.viewports.custom_vp.height, "custom_viewport_height", "Custom Viewport Height")

         CONFIG_BOOL(g_settings.video.smooth, "video_smooth", "Use bilinear filtering")
      END_SUB_GROUP()

      START_SUB_GROUP("Shader")
         CONFIG_BOOL(g_settings.video.shader_enable, "video_shader_enable", "Enable Shaders")
         CONFIG_PATH(g_settings.video.shader_dir, "video_shader_dir", "Shader Directory")
         CONFIG_PATH(g_settings.video.shader_path, "video_shader", "Shader")
      END_SUB_GROUP()

      START_SUB_GROUP("Sync")
         CONFIG_BOOL(g_settings.video.threaded, "video_threaded", "Use threaded video")
         CONFIG_BOOL(g_settings.video.vsync, "video_vsync", "Use VSync")
         CONFIG_BOOL(g_settings.video.hard_sync, "video_hard_sync", "Use OpenGL Hard Sync")
         CONFIG_INT(g_settings.video.hard_sync_frames, "video_hard_sync_frames", "Number of Hard Sync frames") // 0 - 3
      END_SUB_GROUP()

      START_SUB_GROUP("Misc")
         CONFIG_BOOL(g_settings.video.post_filter_record, "video_post_filter_record", "Post filter record")
         CONFIG_BOOL(g_settings.video.gpu_record, "video_gpu_record", "GPU Record")
         CONFIG_BOOL(g_settings.video.gpu_screenshot, "video_gpu_screenshot", "GPU Screenshot")
         CONFIG_BOOL(g_settings.video.allow_rotate, "video_allow_rotate", "Allow rotation")
         CONFIG_BOOL(g_settings.video.crop_overscan, "video_crop_overscan", "Crop Overscan")

         #ifdef HAVE_DYLIB
            CONFIG_PATH(g_settings.video.filter_path, "video_filter", "Software filter"),
         #endif
      END_SUB_GROUP()

      START_SUB_GROUP("Messages")
         CONFIG_PATH(g_settings.video.font_path, "video_font_path", "Font Path")
         CONFIG_FLOAT(g_settings.video.font_size, "video_font_size", "Font Size")
         CONFIG_BOOL(g_settings.video.font_enable, "video_font_enable", "Font Enable")
         CONFIG_BOOL(g_settings.video.font_scale, "video_font_scale", "Font Scale")
         CONFIG_FLOAT(g_settings.video.msg_pos_x, "video_message_pos_x", "Message X Position")
         CONFIG_FLOAT(g_settings.video.msg_pos_y, "video_message_pos_y", "Message Y Position")
         /* message color */
      END_SUB_GROUP()
   END_GROUP()

   /********/
   /* Misc */
   /********/
   START_GROUP("Misc")
      START_SUB_GROUP("Misc")
         CONFIG_BOOL(g_extern.config_save_on_exit, "config_save_on_exit", "Save Config On Exit")
         CONFIG_BOOL(g_settings.network_cmd_enable, "network_cmd_enable", "Network Commands")
         CONFIG_INT(g_settings.network_cmd_port, "network_cmd_port", "Network Command Port")
         CONFIG_BOOL(g_settings.stdin_cmd_enable, "stdin_cmd_enable", "stdin command")
      END_SUB_GROUP()
   END_GROUP()

   { 0 }
};

#endif