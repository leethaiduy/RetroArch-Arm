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


#include "driver.h"
#include "general.h"
#include "file.h"
#include "libretro.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "compat/posix_string.h"
#include "audio/utils.h"
#include "audio/resampler.h"
#include "gfx/thread_wrapper.h"
#include "audio/thread_wrapper.h"
#include "gfx/gfx_common.h"

#ifdef HAVE_X11
#include "gfx/context/x11_common.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static const audio_driver_t *audio_drivers[] = {
#ifdef HAVE_ALSA
   &audio_alsa,
#ifndef __QNX__
   &audio_alsathread,
#endif
#endif
#if defined(HAVE_OSS) || defined(HAVE_OSS_BSD)
   &audio_oss,
#endif
#ifdef HAVE_RSOUND
   &audio_rsound,
#endif
#ifdef HAVE_COREAUDIO
   &audio_coreaudio,
#endif
#ifdef HAVE_AL
   &audio_openal,
#endif
#ifdef HAVE_SL
   &audio_opensl,
#endif
#ifdef HAVE_ROAR
   &audio_roar,
#endif
#ifdef HAVE_JACK
   &audio_jack,
#endif
#ifdef HAVE_SDL
   &audio_sdl,
#endif
#ifdef HAVE_XAUDIO
   &audio_xa,
#endif
#ifdef HAVE_DSOUND
   &audio_dsound,
#endif
#ifdef HAVE_PULSE
   &audio_pulse,
#endif
#ifdef __CELLOS_LV2__
   &audio_ps3,
#endif
#ifdef XENON
   &audio_xenon360,
#endif
#ifdef GEKKO
   &audio_gx,
#endif
#ifdef EMSCRIPTEN
   &audio_rwebaudio,
#endif
#ifdef PSP
   &audio_psp1,
#endif   
#ifdef HAVE_NULLAUDIO
   &audio_null,
#endif
   NULL,
};

static const video_driver_t *video_drivers[] = {
#ifdef HAVE_OPENGL
   &video_gl,
#endif
#ifdef XENON
   &video_xenon360,
#endif
#if defined(_XBOX) && (defined(HAVE_D3D8) || defined(HAVE_D3D9)) || defined(HAVE_WIN32_D3D9)
   &video_d3d,
#endif
#ifdef SN_TARGET_PSP2
   &video_vita,
#endif
#ifdef PSP
   &video_psp1,
#endif
#ifdef HAVE_SDL
   &video_sdl,
#endif
#ifdef HAVE_XVIDEO
   &video_xvideo,
#endif
#ifdef GEKKO
   &video_gx,
#endif
#ifdef HAVE_VG
   &video_vg,
#endif
#ifdef HAVE_NULLVIDEO
   &video_null,
#endif
#ifdef HAVE_LIMA
   &video_lima,
#endif
#ifdef HAVE_OMAP
   &video_omap,
#endif
   NULL,
};

static const input_driver_t *input_drivers[] = {
#ifdef __CELLOS_LV2__
   &input_ps3,
#endif
#if defined(SN_TARGET_PSP2) || defined(PSP)
   &input_psp,
#endif
#ifdef HAVE_SDL
   &input_sdl,
#endif
#ifdef HAVE_DINPUT
   &input_dinput,
#endif
#ifdef HAVE_X11
   &input_x,
#endif
#ifdef XENON
   &input_xenon360,
#endif
#if defined(HAVE_XINPUT2) || defined(HAVE_XINPUT_XBOX1)
   &input_xinput,
#endif
#ifdef GEKKO
   &input_gx,
#endif
#ifdef ANDROID
   &input_android,
#endif
#ifdef HAVE_UDEV
   &input_udev,
#endif
#if defined(__linux__) && !defined(ANDROID)
   &input_linuxraw,
#endif
#if defined(IOS) || defined(OSX) //< Don't use __APPLE__ as it breaks basic SDL builds
   &input_apple,
#endif
#ifdef __BLACKBERRY_QNX__
   &input_qnx,
#endif
#ifdef EMSCRIPTEN
   &input_rwebinput,
#endif
#ifdef HAVE_NULLINPUT
   &input_null,
#endif
   NULL,
};

#ifdef HAVE_OSK
static const input_osk_driver_t *osk_drivers[] = {
#ifdef __CELLOS_LV2__
   &input_ps3_osk,
#endif
   NULL,
};

static int find_osk_driver_index(const char *driver)
{
   unsigned i;
   for (i = 0; osk_drivers[i]; i++)
      if (strcasecmp(driver, osk_drivers[i]->ident) == 0)
         return i;
   return -1;
}

static void find_osk_driver(void)
{
   int i = find_osk_driver_index(g_settings.osk.driver);
   if (i >= 0)
      driver.osk = osk_drivers[i];
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any OSK driver named \"%s\"\n", g_settings.osk.driver);
      RARCH_LOG_OUTPUT("Available OSK drivers are:\n");
      for (d = 0; osk_drivers[d]; d++)
         RARCH_LOG_OUTPUT("\t%s\n", osk_drivers[d]->ident);

      rarch_fail(1, "find_osk_driver()");
   }
}

void find_prev_osk_driver(void)
{
   int i = find_osk_driver_index(g_settings.osk.driver);
   if (i > 0)
      strlcpy(g_settings.osk.driver, osk_drivers[i - 1]->ident, sizeof(g_settings.osk.driver));
   else
      RARCH_WARN("Couldn't find any previous osk driver (current one: \"%s\").\n", g_settings.osk.driver);
}

void find_next_osk_driver(void)
{
   int i = find_osk_driver_index(g_settings.osk.driver);
   if (i >= 0 && osk_drivers[i + 1])
      strlcpy(g_settings.osk.driver, osk_drivers[i + 1]->ident, sizeof(g_settings.osk.driver));
   else
      RARCH_WARN("Couldn't find any next osk driver (current one: \"%s\").\n", g_settings.osk.driver);
}
#endif

#ifdef HAVE_CAMERA
static const camera_driver_t *camera_drivers[] = {
#ifdef HAVE_V4L2
   &camera_v4l2,
#endif
#ifdef EMSCRIPTEN
   &camera_rwebcam,
#endif
#ifdef ANDROID
   &camera_android,
#endif
#ifdef IOS
   &camera_ios,
#endif
   NULL,
};

static int find_camera_driver_index(const char *driver)
{
   unsigned i;
   for (i = 0; camera_drivers[i]; i++)
      if (strcasecmp(driver, camera_drivers[i]->ident) == 0)
         return i;
   return -1;
}

static void find_camera_driver(void)
{
   int i = find_camera_driver_index(g_settings.camera.driver);
   if (i >= 0)
      driver.camera = camera_drivers[i];
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any camera driver named \"%s\"\n", g_settings.camera.driver);
      RARCH_LOG_OUTPUT("Available camera drivers are:\n");
      for (d = 0; camera_drivers[d]; d++)
         RARCH_LOG_OUTPUT("\t%s\n", camera_drivers[d]->ident);

      rarch_fail(1, "find_camera_driver()");
   }
}

void find_prev_camera_driver(void)
{
   int i = find_camera_driver_index(g_settings.camera.driver);
   if (i > 0)
      strlcpy(g_settings.camera.driver, camera_drivers[i - 1]->ident, sizeof(g_settings.camera.driver));
   else
      RARCH_WARN("Couldn't find any previous camera driver (current one: \"%s\").\n", g_settings.camera.driver);
}

void find_next_camera_driver(void)
{
   int i = find_camera_driver_index(g_settings.camera.driver);
   if (i >= 0 && camera_drivers[i + 1])
      strlcpy(g_settings.camera.driver, camera_drivers[i + 1]->ident, sizeof(g_settings.camera.driver));
   else
      RARCH_WARN("Couldn't find any next camera driver (current one: \"%s\").\n", g_settings.camera.driver);
}
#endif

#ifdef HAVE_LOCATION
static const location_driver_t *location_drivers[] = {
#ifdef ANDROID
   &location_android,
#endif
#if defined(IOS) || defined(OSX)
   &location_apple,
#endif
   NULL,
};

static int find_location_driver_index(const char *driver)
{
   unsigned i;
   for (i = 0; location_drivers[i]; i++)
      if (strcasecmp(driver, location_drivers[i]->ident) == 0)
         return i;
   return -1;
}

static void find_location_driver(void)
{
   int i = find_location_driver_index(g_settings.location.driver);
   if (i >= 0)
      driver.location = location_drivers[i];
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any location driver named \"%s\"\n", g_settings.location.driver);
      RARCH_LOG_OUTPUT("Available location drivers are:\n");
      for (d = 0; location_drivers[d]; d++)
         RARCH_LOG_OUTPUT("\t%s\n", location_drivers[d]->ident);

      rarch_fail(1, "find_location_driver()");
   }
}

void find_prev_location_driver(void)
{
   int i = find_location_driver_index(g_settings.location.driver);
   if (i > 0)
      strlcpy(g_settings.location.driver, location_drivers[i - 1]->ident, sizeof(g_settings.location.driver));
   else
      RARCH_WARN("Couldn't find any previous location driver (current one: \"%s\").\n", g_settings.location.driver);
}

void find_next_location_driver(void)
{
   int i = find_location_driver_index(g_settings.location.driver);
   if (i >= 0 && location_drivers[i + 1])
      strlcpy(g_settings.location.driver, location_drivers[i + 1]->ident, sizeof(g_settings.location.driver));
   else
      RARCH_WARN("Couldn't find any next location driver (current one: \"%s\").\n", g_settings.location.driver);
}
#endif

static int find_audio_driver_index(const char *driver)
{
   unsigned i;
   for (i = 0; audio_drivers[i]; i++)
      if (strcasecmp(driver, audio_drivers[i]->ident) == 0)
         return i;
   return -1;
}

static int find_video_driver_index(const char *driver)
{
   unsigned i;
   for (i = 0; video_drivers[i]; i++)
      if (strcasecmp(driver, video_drivers[i]->ident) == 0)
         return i;
   return -1;
}

static int find_input_driver_index(const char *driver)
{
   unsigned i;
   for (i = 0; input_drivers[i]; i++)
      if (strcasecmp(driver, input_drivers[i]->ident) == 0)
         return i;
   return -1;
}

static void find_audio_driver(void)
{
   int i = find_audio_driver_index(g_settings.audio.driver);
   if (i >= 0)
      driver.audio = audio_drivers[i];
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any audio driver named \"%s\"\n", g_settings.audio.driver);
      RARCH_LOG_OUTPUT("Available audio drivers are:\n");
      for (d = 0; audio_drivers[d]; d++)
         RARCH_LOG_OUTPUT("\t%s\n", audio_drivers[d]->ident);

      rarch_fail(1, "find_audio_driver()");
   }
}

void find_prev_audio_driver(void)
{
   int i = find_audio_driver_index(g_settings.audio.driver);
   if (i > 0)
      strlcpy(g_settings.audio.driver, audio_drivers[i - 1]->ident, sizeof(g_settings.audio.driver));
   else
      RARCH_WARN("Couldn't find any previous audio driver (current one: \"%s\").\n", g_settings.audio.driver);
}

void find_next_audio_driver(void)
{
   int i = find_audio_driver_index(g_settings.audio.driver);
   if (i >= 0 && audio_drivers[i + 1])
      strlcpy(g_settings.audio.driver, audio_drivers[i + 1]->ident, sizeof(g_settings.audio.driver));
   else
      RARCH_WARN("Couldn't find any next audio driver (current one: \"%s\").\n", g_settings.audio.driver);
}

static void find_video_driver(void)
{
#if defined(HAVE_OPENGL) && defined(HAVE_FBO)
   if (g_extern.system.hw_render_callback.context_type)
   {
      RARCH_LOG("Using HW render, OpenGL driver forced.\n");
      driver.video = &video_gl;
      return;
   }
#endif

   int i = find_video_driver_index(g_settings.video.driver);
   if (i >= 0)
      driver.video = video_drivers[i];
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any video driver named \"%s\"\n", g_settings.video.driver);
      RARCH_LOG_OUTPUT("Available video drivers are:\n");
      for (d = 0; video_drivers[d]; d++)
         RARCH_LOG_OUTPUT("\t%s\n", video_drivers[d]->ident);

      rarch_fail(1, "find_video_driver()");
   }
}

void find_prev_video_driver(void)
{
   // No need to enforce GL if HW render. This is done at driver init anyways.
   int i = find_video_driver_index(g_settings.video.driver);
   if (i > 0)
      strlcpy(g_settings.video.driver, video_drivers[i - 1]->ident, sizeof(g_settings.video.driver));
   else
      RARCH_WARN("Couldn't find any previous video driver (current one: \"%s\").\n", g_settings.video.driver);
}

void find_next_video_driver(void)
{
   // No need to enforce GL if HW render. This is done at driver init anyways.
   int i = find_video_driver_index(g_settings.video.driver);
   if (i >= 0 && video_drivers[i + 1])
      strlcpy(g_settings.video.driver, video_drivers[i + 1]->ident, sizeof(g_settings.video.driver));
   else
      RARCH_WARN("Couldn't find any next video driver (current one: \"%s\").\n", g_settings.video.driver);
}

static void find_input_driver(void)
{
   int i = find_input_driver_index(g_settings.input.driver);
   if (i >= 0)
      driver.input = input_drivers[i];
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any input driver named \"%s\"\n", g_settings.input.driver);
      RARCH_LOG_OUTPUT("Available input drivers are:\n");
      for (d = 0; input_drivers[d]; d++)
         RARCH_LOG_OUTPUT("\t%s\n", input_drivers[d]->ident);

      rarch_fail(1, "find_input_driver()");
   }
}

void find_prev_input_driver(void)
{
   int i = find_input_driver_index(g_settings.input.driver);
   if (i > 0)
      strlcpy(g_settings.input.driver, input_drivers[i - 1]->ident, sizeof(g_settings.input.driver));
   else
      RARCH_ERR("Couldn't find any previous input driver (current one: \"%s\").\n", g_settings.input.driver);
}

void find_next_input_driver(void)
{
   int i = find_input_driver_index(g_settings.input.driver);
   if (i >= 0 && input_drivers[i + 1])
      strlcpy(g_settings.input.driver, input_drivers[i + 1]->ident, sizeof(g_settings.input.driver));
   else
      RARCH_ERR("Couldn't find any next input driver (current one: \"%s\").\n", g_settings.input.driver);
}

void init_drivers_pre(void)
{
   find_audio_driver();
   find_video_driver();
   find_input_driver();
#ifdef HAVE_CAMERA
   find_camera_driver();
#endif
#ifdef HAVE_LOCATION
   find_location_driver();
#endif
#ifdef HAVE_OSK
   find_osk_driver();
#endif
}

static void adjust_system_rates(void)
{
   g_extern.system.force_nonblock = false;
   const struct retro_system_timing *info = &g_extern.system.av_info.timing;

   if (info->fps <= 0.0 || info->sample_rate <= 0.0)
      return;

   float timing_skew = fabs(1.0f - info->fps / g_settings.video.refresh_rate);
   if (timing_skew > 0.05f) // We don't want to adjust pitch too much. If we have extreme cases, just don't readjust at all.
   {
      RARCH_LOG("Timings deviate too much. Will not adjust. (Display = %.2f Hz, Game = %.2f Hz)\n",
            g_settings.video.refresh_rate,
            (float)info->fps);

      // We won't be able to do VSync reliably as game FPS > monitor FPS.
      if (info->fps > g_settings.video.refresh_rate)
      {
         g_extern.system.force_nonblock = true;
         RARCH_LOG("Game FPS > Monitor FPS. Cannot rely on VSync.\n");
      }

      g_settings.audio.in_rate = info->sample_rate;
   }
   else
      g_settings.audio.in_rate = info->sample_rate *
         (g_settings.video.refresh_rate / info->fps);

   RARCH_LOG("Set audio input rate to: %.2f Hz.\n", g_settings.audio.in_rate);

   if (driver.video_data)
   {
      if (g_extern.system.force_nonblock)
         video_set_nonblock_state_func(true);
      else
         driver_set_nonblock_state(driver.nonblock_state);
   }
}

void driver_set_monitor_refresh_rate(float hz)
{
   char msg[256];
   snprintf(msg, sizeof(msg), "Setting refresh rate to: %.3f Hz.", hz);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   RARCH_LOG("%s\n", msg);

   g_settings.video.refresh_rate = hz;
   adjust_system_rates();

   g_extern.audio_data.orig_src_ratio =
      g_extern.audio_data.src_ratio =
      (double)g_settings.audio.out_rate / g_settings.audio.in_rate;
}

void driver_set_nonblock_state(bool nonblock)
{
   // Only apply non-block-state for video if we're using vsync.
   if (g_extern.video_active && driver.video_data)
   {
      bool video_nb = nonblock;
      if (!g_settings.video.vsync || g_extern.system.force_nonblock)
         video_nb = true;
      video_set_nonblock_state_func(video_nb);
   }

   if (g_extern.audio_active && driver.audio_data)
      audio_set_nonblock_state_func(g_settings.audio.sync ? nonblock : true);

   g_extern.audio_data.chunk_size = nonblock ?
      g_extern.audio_data.nonblock_chunk_size : g_extern.audio_data.block_chunk_size;
}

bool driver_set_rumble_state(unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   if (driver.input && driver.input_data && driver.input->set_rumble)
      return driver.input->set_rumble(driver.input_data, port, effect, strength);
   else
      return false;
}

bool driver_set_sensor_state(unsigned port, enum retro_sensor_action action, unsigned rate)
{
   if (driver.input && driver.input_data && driver.input->set_sensor_state)
      return driver.input->set_sensor_state(driver.input_data, port, action, rate);
   else
      return false;
}

float driver_sensor_get_input(unsigned port, unsigned id)
{
   if (driver.input && driver.input_data && driver.input->get_sensor_input)
      return driver.input->get_sensor_input(driver.input_data, port, id);
   else
      return 0.0f;
}

#ifdef HAVE_CAMERA
bool driver_camera_start(void)
{
   if (driver.camera && driver.camera_data && g_settings.camera.allow)
      return driver.camera->start(driver.camera_data);
   else
      return false;
}

void driver_camera_stop(void)
{
   if (driver.camera && driver.camera_data)
      driver.camera->stop(driver.camera_data);
}

void driver_camera_poll(void)
{
   if (driver.camera && driver.camera_data)
   {
      driver.camera->poll(driver.camera_data,
            g_extern.system.camera_callback.frame_raw_framebuffer,
            g_extern.system.camera_callback.frame_opengl_texture);
   }
}
#endif

#ifdef HAVE_LOCATION
bool driver_location_start(void)
{
   if (driver.location && driver.location_data && g_settings.location.allow)
      return driver.location->start(driver.location_data);
   else
      return false;
}

void driver_location_stop(void)
{
   if (driver.location && driver.location_data)
      driver.location->stop(driver.location_data);
}

void driver_location_set_interval(unsigned interval_msecs, unsigned interval_distance)
{
   if (driver.location && driver.location_data)
      driver.location->set_interval(driver.location_data, interval_msecs, interval_distance);
}

bool driver_location_get_position(double *lat, double *lon, double *horiz_accuracy,
      double *vert_accuracy)
{
   if (driver.location && driver.location_data)
      return driver.location->get_position(driver.location_data, lat, lon, horiz_accuracy, vert_accuracy);

   *lat = 0.0;
   *lon = 0.0;
   *horiz_accuracy = 0.0;
   *vert_accuracy = 0.0;
   return false;
}
#endif

uintptr_t driver_get_current_framebuffer(void)
{
#ifdef HAVE_FBO
   if (driver.video_poke && driver.video_poke->get_current_framebuffer)
      return driver.video_poke->get_current_framebuffer(driver.video_data);
   else
#endif
      return 0;
}

retro_proc_address_t driver_get_proc_address(const char *sym)
{
#ifdef HAVE_FBO
   if (driver.video_poke && driver.video_poke->get_proc_address)
      return driver.video_poke->get_proc_address(driver.video_data, sym);
   else
#endif
      return NULL;
}

bool driver_update_system_av_info(const struct retro_system_av_info *info)
{
   g_extern.system.av_info = *info;
   rarch_set_fullscreen(g_settings.video.fullscreen);
   // Cannot continue recording with different parameters.
   // Take the easiest route out and just restart the recording.
#ifdef HAVE_FFMPEG
   if (g_extern.recording)
   {
      static const char *msg = "Restarting FFmpeg recording due to driver reinit.";
      msg_queue_push(g_extern.msg_queue, msg, 2, 180);
      RARCH_WARN("%s\n", msg);
      rarch_deinit_recording();
      rarch_init_recording();
   }
#endif
   return true;
}

// Only called once on init and deinit.
// Video and input drivers need to be active (owned)
// before retroarch core starts.
// Core handles audio.

void global_init_drivers(void)
{
   find_audio_driver();
   find_input_driver();
   init_video_input();
}

void global_uninit_drivers(void)
{
   if (driver.video && driver.video_data)
   {
      driver.video->free(driver.video_data);
      driver.video_data = NULL;
   }

   if (driver.input && driver.input_data)
   {
      driver.input->free(driver.input_data);
      driver.input_data = NULL;
   }

#ifdef HAVE_CAMERA
   if (driver.camera && driver.camera_data)
   {
      if (g_extern.system.camera_callback.deinitialized)
         g_extern.system.camera_callback.deinitialized();
      driver.camera->free(driver.camera_data);
      driver.camera_data = NULL;
   }
#endif

#ifdef HAVE_LOCATION
   if (driver.location && driver.location_data)
   {
      if (g_extern.system.location_callback.deinitialized)
         g_extern.system.location_callback.deinitialized();
      driver.location->free(driver.location_data);
      driver.location_data = NULL;
   }
#endif

#ifdef HAVE_OSK
   if (driver.osk && driver.osk_data)
   {
      driver.osk->free(driver.osk_data);
      driver.osk_data = NULL;
   }
#endif
}

#ifdef HAVE_CAMERA
void init_camera(void)
{
   // Resource leaks will follow if camera is initialized twice.
   if (driver.camera_data)
      return;

   find_camera_driver();

   driver.camera_data = camera_init_func(
         *g_settings.camera.device ? g_settings.camera.device : NULL,
         g_extern.system.camera_callback.caps,
         g_settings.camera.width ? g_settings.camera.width : g_extern.system.camera_callback.width,
         g_settings.camera.height ? g_settings.camera.height : g_extern.system.camera_callback.height);

   if (!driver.camera_data)
   {
      RARCH_ERR("Failed to initialize camera driver. Will continue without camera.\n");
      g_extern.camera_active = false;
   }

   if (g_extern.system.camera_callback.initialized)
      g_extern.system.camera_callback.initialized();
}
#endif

#ifdef HAVE_LOCATION
void init_location(void)
{
   // Resource leaks will follow if location interface is initialized twice.
   if (driver.location_data)
      return;

   find_location_driver();

   driver.location_data = location_init_func();

   if (!driver.location_data)
   {
      RARCH_ERR("Failed to initialize location driver. Will continue without location.\n");
      g_extern.location_active = false;
   }

   if (g_extern.system.location_callback.initialized)
      g_extern.system.location_callback.initialized();
}
#endif

#ifdef HAVE_OSK
void init_osk(void)
{
   // Resource leaks will follow if osk is initialized twice.
   if (driver.osk_data)
      return;

   find_osk_driver();

   //FIXME - refactor params later based on semantics 
   driver.osk_data = osk_init_func(0);

   if (!driver.osk_data)
   {
      RARCH_ERR("Failed to initialize OSK driver. Will continue without OSK.\n");
      g_extern.osk_active = false;
   }
}
#endif

void init_drivers(void)
{
   driver.video_data_own = !driver.video_data;
   driver.audio_data_own = !driver.audio_data;
   driver.input_data_own = !driver.input_data;
#ifdef HAVE_CAMERA
   driver.camera_data_own = !driver.camera_data;
#endif
#ifdef HAVE_LOCATION
   driver.location_data_own = !driver.location_data;
#endif
#ifdef HAVE_OSK
   driver.osk_data_own = !driver.osk_data;
#endif

   adjust_system_rates();

   g_extern.frame_count = 0;
   init_video_input();

   if (!driver.video_cache_context_ack && g_extern.system.hw_render_callback.context_reset)
      g_extern.system.hw_render_callback.context_reset();
   driver.video_cache_context_ack = false;

   init_audio();

#ifdef HAVE_CAMERA
   // Only init camera driver if we're ever going to use it.
   if (g_extern.camera_active)
      init_camera();
#endif

#ifdef HAVE_LOCATION
   // Only init location driver if we're ever going to use it.
   if (g_extern.location_active)
      init_location();
#endif

#ifdef HAVE_OSK
   init_osk();
#endif

   // Keep non-throttled state as good as possible.
   if (driver.nonblock_state)
      driver_set_nonblock_state(driver.nonblock_state);

   g_extern.system.frame_time_last = 0;
}

#ifdef HAVE_CAMERA
void uninit_camera(void)
{
   if (driver.camera_data && driver.camera)
   {
      if (g_extern.system.camera_callback.deinitialized)
         g_extern.system.camera_callback.deinitialized();
      driver.camera->free(driver.camera_data);
   }
}
#endif

#ifdef HAVE_LOCATION
void uninit_location(void)
{
   if (driver.location_data && driver.location)
   {
      if (g_extern.system.location_callback.deinitialized)
         g_extern.system.location_callback.deinitialized();
      driver.location->free(driver.location_data);
   }
}
#endif

#ifdef HAVE_OSK
void uninit_osk(void)
{
   if (driver.osk_data && driver.osk)
      driver.osk->free(driver.osk_data);
}
#endif

void uninit_drivers(void)
{
   uninit_audio();

   if (g_extern.system.hw_render_callback.context_destroy && !driver.video_cache_context)
      g_extern.system.hw_render_callback.context_destroy();

   uninit_video_input();

#ifdef HAVE_CAMERA
   uninit_camera();

   if (driver.camera_data_own)
      driver.camera_data = NULL;
#endif

#ifdef HAVE_LOCATION
   uninit_location();

   if (driver.location_data_own)
      driver.location_data = NULL;
#endif
   
#ifdef HAVE_OSK
   uninit_osk();

   if (driver.osk_data_own)
      driver.osk_data = NULL;
#endif
   if (driver.video_data_own)
      driver.video_data = NULL;
   if (driver.audio_data_own)
      driver.audio_data = NULL;
   if (driver.input_data_own)
      driver.input_data = NULL;

#ifdef HAVE_CAMERA
   driver.camera_data_own = false;
#endif
#ifdef HAVE_LOCATION
   driver.location_data_own = false;
#endif
#ifdef HAVE_OSK
   driver.osk_data_own    = false;
#endif
   driver.video_data_own  = false;
   driver.audio_data_own  = false;
   driver.input_data_own  = false;
}

#ifdef HAVE_DYLIB
static void init_dsp_plugin(void)
{
   if (!(*g_settings.audio.dsp_plugin))
      return;

   rarch_dsp_info_t info = {0};

   g_extern.audio_data.dsp_lib = dylib_load(g_settings.audio.dsp_plugin);
   if (!g_extern.audio_data.dsp_lib)
   {
      RARCH_ERR("Failed to open DSP plugin: \"%s\" ...\n", g_settings.audio.dsp_plugin);
      return;
   }

   const rarch_dsp_plugin_t* (RARCH_API_CALLTYPE *plugin_init)(void) = 
      (const rarch_dsp_plugin_t *(RARCH_API_CALLTYPE*)(void))dylib_proc(g_extern.audio_data.dsp_lib, "rarch_dsp_plugin_init");

   if (!plugin_init)
   {
      RARCH_ERR("Failed to find symbol \"rarch_dsp_plugin_init\" in DSP plugin.\n");
      goto error;
   }

   g_extern.audio_data.dsp_plugin = plugin_init();
   if (!g_extern.audio_data.dsp_plugin)
   {
      RARCH_ERR("Failed to get a valid DSP plugin.\n");
      goto error;
   }

   if (g_extern.audio_data.dsp_plugin->api_version != RARCH_DSP_API_VERSION)
   {
      RARCH_ERR("DSP plugin API mismatch. RetroArch: %d, Plugin: %d\n", RARCH_DSP_API_VERSION, g_extern.audio_data.dsp_plugin->api_version);
      goto error;
   }

   RARCH_LOG("Loaded DSP plugin: \"%s\"\n", g_extern.audio_data.dsp_plugin->ident ? g_extern.audio_data.dsp_plugin->ident : "Unknown");

   info.input_rate = g_settings.audio.in_rate;

   g_extern.audio_data.dsp_handle = g_extern.audio_data.dsp_plugin->init(&info);
   if (!g_extern.audio_data.dsp_handle)
   {
      RARCH_ERR("Failed to init DSP plugin.\n");
      goto error;
   }

   return;

error:
   if (g_extern.audio_data.dsp_lib)
      dylib_close(g_extern.audio_data.dsp_lib);
   g_extern.audio_data.dsp_plugin = NULL;
   g_extern.audio_data.dsp_lib = NULL;
}

static void deinit_dsp_plugin(void)
{
   if (g_extern.audio_data.dsp_lib && g_extern.audio_data.dsp_plugin)
   {
      g_extern.audio_data.dsp_plugin->free(g_extern.audio_data.dsp_handle);
      dylib_close(g_extern.audio_data.dsp_lib);
   }
}
#endif

void init_audio(void)
{
   audio_convert_init_simd();

   // Resource leaks will follow if audio is initialized twice.
   if (driver.audio_data)
      return;

   // Accomodate rewind since at some point we might have two full buffers.
   size_t max_bufsamples = AUDIO_CHUNK_SIZE_NONBLOCKING * 2;
   size_t outsamples_max = max_bufsamples * AUDIO_MAX_RATIO * g_settings.slowmotion_ratio;

   // Used for recording even if audio isn't enabled.
   rarch_assert(g_extern.audio_data.conv_outsamples = (int16_t*)malloc(outsamples_max * sizeof(int16_t)));

   g_extern.audio_data.block_chunk_size    = AUDIO_CHUNK_SIZE_BLOCKING;
   g_extern.audio_data.nonblock_chunk_size = AUDIO_CHUNK_SIZE_NONBLOCKING;
   g_extern.audio_data.chunk_size          = g_extern.audio_data.block_chunk_size;

   // Needs to be able to hold full content of a full max_bufsamples in addition to its own.
   rarch_assert(g_extern.audio_data.rewind_buf = (int16_t*)malloc(max_bufsamples * sizeof(int16_t)));
   g_extern.audio_data.rewind_size             = max_bufsamples;

   if (!g_settings.audio.enable)
   {
      g_extern.audio_active = false;
      return;
   }

#ifdef HAVE_THREADS
   find_audio_driver();
   if (g_extern.system.audio_callback.callback)
   {
      RARCH_LOG("Starting threaded audio driver ...\n");
      if (!rarch_threaded_audio_init(&driver.audio, &driver.audio_data,
               *g_settings.audio.device ? g_settings.audio.device : NULL,
               g_settings.audio.out_rate, g_settings.audio.latency,
               driver.audio))
      {
         RARCH_ERR("Cannot open threaded audio driver ... Exiting ...\n");
         rarch_fail(1, "init_audio()");
      }
   }
   else
#endif
   {
      driver.audio_data = audio_init_func(*g_settings.audio.device ? g_settings.audio.device : NULL,
            g_settings.audio.out_rate, g_settings.audio.latency);
   }

   if (!driver.audio_data)
   {
      RARCH_ERR("Failed to initialize audio driver. Will continue without audio.\n");
      g_extern.audio_active = false;
   }

   g_extern.audio_data.use_float = false;
   if (g_extern.audio_active && driver.audio->use_float && audio_use_float_func())
      g_extern.audio_data.use_float = true;

   if (!g_settings.audio.sync && g_extern.audio_active)
   {
      audio_set_nonblock_state_func(true);
      g_extern.audio_data.chunk_size = g_extern.audio_data.nonblock_chunk_size;
   }

   g_extern.audio_data.orig_src_ratio =
      g_extern.audio_data.src_ratio =
      (double)g_settings.audio.out_rate / g_settings.audio.in_rate;

   if (!rarch_resampler_realloc(&g_extern.audio_data.resampler_data, &g_extern.audio_data.resampler,
         g_settings.audio.resampler, g_extern.audio_data.orig_src_ratio))
   {
      RARCH_ERR("Failed to initialize resampler \"%s\".\n", g_settings.audio.resampler);
      g_extern.audio_active = false;
   }

   rarch_assert(g_extern.audio_data.data = (float*)malloc(max_bufsamples * sizeof(float)));

   g_extern.audio_data.data_ptr = 0;

   rarch_assert(g_settings.audio.out_rate < g_settings.audio.in_rate * AUDIO_MAX_RATIO);
   rarch_assert(g_extern.audio_data.outsamples = (float*)malloc(outsamples_max * sizeof(float)));

   g_extern.audio_data.rate_control = false;
   if (!g_extern.system.audio_callback.callback && g_extern.audio_active && g_settings.audio.rate_control)
   {
      if (driver.audio->buffer_size && driver.audio->write_avail)
      {
         g_extern.audio_data.driver_buffer_size = audio_buffer_size_func();
         g_extern.audio_data.rate_control = true;
      }
      else
         RARCH_WARN("Audio rate control was desired, but driver does not support needed features.\n");
   }

#ifdef HAVE_DYLIB
   init_dsp_plugin();
#endif

   g_extern.measure_data.buffer_free_samples_count = 0;

   if (g_extern.audio_active && !g_extern.audio_data.mute && g_extern.system.audio_callback.callback) // Threaded driver is initially stopped.
      audio_start_func();
}


static void compute_audio_buffer_statistics(void)
{
   unsigned i, samples;
   samples = min(g_extern.measure_data.buffer_free_samples_count, AUDIO_BUFFER_FREE_SAMPLES_COUNT);
   if (samples < 3)
      return;

   uint64_t accum = 0;
   for (i = 1; i < samples; i++)
      accum += g_extern.measure_data.buffer_free_samples[i];

   int avg = accum / (samples - 1);

   uint64_t accum_var = 0;
   for (i = 1; i < samples; i++)
   {
      int diff = avg - g_extern.measure_data.buffer_free_samples[i];
      accum_var += diff * diff;
   }

   unsigned stddev = (unsigned)sqrt((double)accum_var / (samples - 2));

   float avg_filled = 1.0f - (float)avg / g_extern.audio_data.driver_buffer_size;
   float deviation = (float)stddev / g_extern.audio_data.driver_buffer_size;

   unsigned low_water_size = g_extern.audio_data.driver_buffer_size * 3 / 4;
   unsigned high_water_size = g_extern.audio_data.driver_buffer_size / 4;

   unsigned low_water_count = 0;
   unsigned high_water_count = 0;
   for (i = 1; i < samples; i++)
   {
      if (g_extern.measure_data.buffer_free_samples[i] >= low_water_size)
         low_water_count++;
      else if (g_extern.measure_data.buffer_free_samples[i] <= high_water_size)
         high_water_count++;
   }

   RARCH_LOG("Average audio buffer saturation: %.2f %%, standard deviation (percentage points): %.2f %%.\n",
         avg_filled * 100.0, deviation * 100.0);
   RARCH_LOG("Amount of time spent close to underrun: %.2f %%. Close to blocking: %.2f %%.\n",
         (100.0 * low_water_count) / (samples - 1),
         (100.0 * high_water_count) / (samples - 1));
}

bool driver_monitor_fps_statistics(double *refresh_rate, double *deviation, unsigned *sample_points)
{
   unsigned i;
   if (g_settings.video.threaded)
      return false;

   unsigned samples = min(MEASURE_FRAME_TIME_SAMPLES_COUNT, g_extern.measure_data.frame_time_samples_count);
   if (samples < 2)
      return false;

   // Measure statistics on frame time (microsecs), *not* FPS.
   retro_time_t accum = 0;
   for (i = 0; i < samples; i++)
      accum += g_extern.measure_data.frame_time_samples[i];

#if 0
   for (i = 0; i < samples; i++)
      RARCH_LOG("Interval #%u: %d usec / frame.\n",
            i, (int)g_extern.measure_data.frame_time_samples[i]);
#endif

   retro_time_t avg = accum / samples;
   retro_time_t accum_var = 0;

   // Drop first measurement. It is likely to be bad.
   for (i = 0; i < samples; i++)
   {
      retro_time_t diff = g_extern.measure_data.frame_time_samples[i] - avg;
      accum_var += diff * diff;
   }

   *deviation = sqrt((double)accum_var / (samples - 1)) / avg;
   *refresh_rate = 1000000.0 / avg;
   *sample_points = samples;

   return true;
}

static void compute_monitor_fps_statistics(void)
{
   if (g_settings.video.threaded)
   {
      RARCH_LOG("Monitor FPS estimation is disabled for threaded video.\n");
      return;
   }

   if (g_extern.measure_data.frame_time_samples_count < 2 * MEASURE_FRAME_TIME_SAMPLES_COUNT)
   {
      RARCH_LOG("Does not have enough samples for monitor refresh rate estimation. Requires to run for at least %u frames.\n",
            2 * MEASURE_FRAME_TIME_SAMPLES_COUNT);
      return;
   }

   double avg_fps = 0.0;
   double stddev = 0.0;
   unsigned samples = 0;
   if (driver_monitor_fps_statistics(&avg_fps, &stddev, &samples))
   {
      RARCH_LOG("Average monitor Hz: %.6f Hz. (%.3f %% frame time deviation, based on %u last samples).\n",
            avg_fps, 100.0 * stddev, samples);
   }
}

void uninit_audio(void)
{
   if (driver.audio_data && driver.audio)
      driver.audio->free(driver.audio_data);

   free(g_extern.audio_data.conv_outsamples);
   g_extern.audio_data.conv_outsamples = NULL;
   g_extern.audio_data.data_ptr        = 0;

   free(g_extern.audio_data.rewind_buf);
   g_extern.audio_data.rewind_buf = NULL;

   if (!g_settings.audio.enable)
   {
      g_extern.audio_active = false;
      return;
   }

   rarch_resampler_freep(&g_extern.audio_data.resampler, &g_extern.audio_data.resampler_data);

   free(g_extern.audio_data.data);
   g_extern.audio_data.data = NULL;

   free(g_extern.audio_data.outsamples);
   g_extern.audio_data.outsamples = NULL;

#ifdef HAVE_DYLIB
   deinit_dsp_plugin();
#endif

   compute_audio_buffer_statistics();
}

#ifdef HAVE_DYLIB
static void deinit_filter(void)
{
   g_extern.filter.active = false;

   if (g_extern.filter.lib)
      dylib_close(g_extern.filter.lib);
   g_extern.filter.lib = NULL;

   free(g_extern.filter.buffer);
   free(g_extern.filter.colormap);
   free(g_extern.filter.scaler_out);
   g_extern.filter.buffer     = NULL;
   g_extern.filter.colormap   = NULL;
   g_extern.filter.scaler_out = NULL;

   scaler_ctx_gen_reset(&g_extern.filter.scaler);
   memset(&g_extern.filter.scaler, 0, sizeof(g_extern.filter.scaler));
}

static void init_filter(bool rgb32)
{
   unsigned i;
   if (g_extern.filter.active)
      return;
   if (!*g_settings.video.filter_path)
      return;

   if (g_extern.system.hw_render_callback.context_type)
   {
      RARCH_WARN("Cannot use CPU filters when hardware rendering is used.\n");
      return;
   }

   RARCH_LOG("Loading bSNES filter from \"%s\"\n", g_settings.video.filter_path);
   g_extern.filter.lib = dylib_load(g_settings.video.filter_path);
   if (!g_extern.filter.lib)
   {
      RARCH_ERR("Failed to load filter \"%s\"\n", g_settings.video.filter_path);
      return;
   }

   struct retro_game_geometry *geom = &g_extern.system.av_info.geometry;
   unsigned width   = geom->max_width;
   unsigned height  = geom->max_height;
   unsigned pow2_x  = 0;
   unsigned pow2_y  = 0;
   unsigned maxsize = 0;

   g_extern.filter.psize = 
      (void (*)(unsigned*, unsigned*))dylib_proc(g_extern.filter.lib, "filter_size");
   g_extern.filter.prender = 
      (void (*)(uint32_t*, uint32_t*, 
                unsigned, const uint16_t*, 
                unsigned, unsigned, unsigned))dylib_proc(g_extern.filter.lib, "filter_render");

   if (!g_extern.filter.psize || !g_extern.filter.prender)
   {
      RARCH_ERR("Failed to find functions in filter...\n");
      goto error;
   }

   g_extern.filter.active = true;
   g_extern.filter.psize(&width, &height);

   pow2_x  = next_pow2(width);
   pow2_y  = next_pow2(height);
   maxsize = pow2_x > pow2_y ? pow2_x : pow2_y; 
   g_extern.filter.scale = maxsize / RARCH_SCALE_BASE;

   g_extern.filter.buffer = (uint32_t*)malloc(RARCH_SCALE_BASE * RARCH_SCALE_BASE *
         g_extern.filter.scale * g_extern.filter.scale * sizeof(uint32_t));
   if (!g_extern.filter.buffer)
      goto error;

   g_extern.filter.pitch = RARCH_SCALE_BASE * g_extern.filter.scale * sizeof(uint32_t);

   g_extern.filter.colormap = (uint32_t*)malloc(0x10000 * sizeof(uint32_t));
   if (!g_extern.filter.colormap)
      goto error;

   // Set up conversion map from 16-bit XRGB1555 to 32-bit ARGB.
   for (i = 0; i < 0x10000; i++)
   {
      unsigned r = (i >> 10) & 0x1f;
      unsigned g = (i >>  5) & 0x1f;
      unsigned b = (i >>  0) & 0x1f;

      r = (r << 3) | (r >> 2);
      g = (g << 3) | (g >> 2);
      b = (b << 3) | (b >> 2);
      g_extern.filter.colormap[i] = (r << 16) | (g << 8) | (b << 0);
   }

   g_extern.filter.scaler_out = (uint16_t*)malloc(sizeof(uint16_t) * geom->max_width * geom->max_height);
   if (!g_extern.filter.scaler_out)
      goto error;

   g_extern.filter.scaler.scaler_type = SCALER_TYPE_POINT;
   g_extern.filter.scaler.in_fmt      = rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565;
   g_extern.filter.scaler.out_fmt     = SCALER_FMT_0RGB1555;

   if (!scaler_ctx_gen_filter(&g_extern.filter.scaler))
      goto error;

   return;

error:
   RARCH_ERR("CPU filter init failed.\n");
   deinit_filter();
}
#endif

static void deinit_shader_dir(void)
{
   // It handles NULL, no worries :D
   dir_list_free(g_extern.shader_dir.list);
   g_extern.shader_dir.list = NULL;
   g_extern.shader_dir.ptr  = 0;
}

static void init_shader_dir(void)
{
   unsigned i;
   if (!*g_settings.video.shader_dir)
      return;

   g_extern.shader_dir.list = dir_list_new(g_settings.video.shader_dir, "shader|cg|cgp|glsl|glslp", false);
   if (!g_extern.shader_dir.list || g_extern.shader_dir.list->size == 0)
   {
      deinit_shader_dir();
      return;
   }

   g_extern.shader_dir.ptr  = 0;
   dir_list_sort(g_extern.shader_dir.list, false);

   for (i = 0; i < g_extern.shader_dir.list->size; i++)
      RARCH_LOG("Found shader \"%s\"\n", g_extern.shader_dir.list->elems[i].data);
}

static void deinit_pixel_converter(void)
{
   scaler_ctx_gen_reset(&driver.scaler);
   memset(&driver.scaler, 0, sizeof(driver.scaler));
   free(driver.scaler_out);
   driver.scaler_out = NULL;
}

static bool init_video_pixel_converter(unsigned size)
{
   // This function can be called multiple times without deiniting first on consoles.
   deinit_pixel_converter();

   if (g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_0RGB1555)
   {
      RARCH_WARN("0RGB1555 pixel format is deprecated, and will be slower. For 15/16-bit, RGB565 format is preferred.\n");

      driver.scaler.scaler_type = SCALER_TYPE_POINT;
      driver.scaler.in_fmt      = SCALER_FMT_0RGB1555;

      // TODO: Pick either ARGB8888 or RGB565 depending on driver ...
      driver.scaler.out_fmt     = SCALER_FMT_RGB565;

      if (!scaler_ctx_gen_filter(&driver.scaler))
         return false;

      driver.scaler_out = calloc(sizeof(uint16_t), size * size);
   }

   return true;
}

void init_video_input(void)
{
#ifdef HAVE_DYLIB
   init_filter(g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);
#endif

   init_shader_dir();

   const struct retro_game_geometry *geom = &g_extern.system.av_info.geometry;
   unsigned max_dim = max(geom->max_width, geom->max_height);
   unsigned scale = next_pow2(max_dim) / RARCH_SCALE_BASE;
   scale = max(scale, 1);

   if (g_extern.filter.active)
      scale = g_extern.filter.scale;

   // Update core-dependent aspect ratio values.
   gfx_set_square_pixel_viewport(geom->base_width, geom->base_height);
   gfx_set_core_viewport();
   gfx_set_config_viewport();

   // Update CUSTOM viewport.
   rarch_viewport_t *custom_vp = &g_extern.console.screen.viewports.custom_vp;
   if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      float default_aspect = aspectratio_lut[ASPECT_RATIO_CORE].value;
      aspectratio_lut[ASPECT_RATIO_CUSTOM].value = (custom_vp->width && custom_vp->height) ?
         (float)custom_vp->width / custom_vp->height : default_aspect;
   }

   g_extern.system.aspect_ratio = aspectratio_lut[g_settings.video.aspect_ratio_idx].value;

   unsigned width;
   unsigned height;
   if (g_settings.video.fullscreen)
   {
      width = g_settings.video.fullscreen_x;
      height = g_settings.video.fullscreen_y;
   }
   else
   {
      if (g_settings.video.force_aspect)
      {
         // Do rounding here to simplify integer scale correctness.
         unsigned base_width = roundf(geom->base_height * g_extern.system.aspect_ratio);
         width = roundf(base_width * g_settings.video.xscale);
         height = roundf(geom->base_height * g_settings.video.yscale);
      }
      else
      {
         width = roundf(geom->base_width * g_settings.video.xscale);
         height = roundf(geom->base_height * g_settings.video.yscale);
      }
   }

   if (width && height)
      RARCH_LOG("Video @ %ux%u\n", width, height);
   else
      RARCH_LOG("Video @ fullscreen\n");

   driver.display_type  = RARCH_DISPLAY_NONE;
   driver.video_display = 0;
   driver.video_window  = 0;

   if (!init_video_pixel_converter(RARCH_SCALE_BASE * scale))
   {
      RARCH_ERR("Failed to init pixel converter.\n");
      rarch_fail(1, "init_video_input()");
   }

   video_info_t video = {0};
   video.width = width;
   video.height = height;
   video.fullscreen = g_settings.video.fullscreen;
   video.vsync = g_settings.video.vsync && !g_extern.system.force_nonblock;
   video.force_aspect = g_settings.video.force_aspect;
   video.smooth = g_settings.video.smooth;
   video.input_scale = scale;
   video.rgb32 = g_extern.filter.active || (g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);

   const input_driver_t *tmp = driver.input;
   find_video_driver(); // Need to grab the "real" video driver interface on a reinit.
#ifdef HAVE_THREADS
   if (g_settings.video.threaded && !g_extern.system.hw_render_callback.context_type) // Can't do hardware rendering with threaded driver currently.
   {
      RARCH_LOG("Starting threaded video driver ...\n");
      if (!rarch_threaded_video_init(&driver.video, &driver.video_data,
               &driver.input, &driver.input_data,
               driver.video, &video))
      {
         RARCH_ERR("Cannot open threaded video driver ... Exiting ...\n");
         rarch_fail(1, "init_video_input()");
      }
   }
   else
#endif
      driver.video_data = video_init_func(&video, &driver.input, &driver.input_data);

   if (driver.video_data == NULL)
   {
      RARCH_ERR("Cannot open video driver ... Exiting ...\n");
      rarch_fail(1, "init_video_input()");
   }

   driver.video_poke = NULL;
   if (driver.video->poke_interface)
      driver.video->poke_interface(driver.video_data, &driver.video_poke);

   // Force custom viewport to have sane parameters.
   if (driver.video->viewport_info && (!custom_vp->width || !custom_vp->height))
   {
      custom_vp->width = width;
      custom_vp->height = height;
      driver.video->viewport_info(driver.video_data, custom_vp);
   }

   if (driver.video->set_rotation)
      video_set_rotation_func((g_settings.video.rotation + g_extern.system.rotation) % 4);

#ifdef HAVE_X11
   if (driver.display_type == RARCH_DISPLAY_X11)
   {
      RARCH_LOG("Suspending screensaver (X11).\n");
      x11_suspend_screensaver(driver.video_window);
   }
#endif

   // Video driver didn't provide an input driver so we use configured one.
   if (driver.input == NULL)
   {
      RARCH_LOG("Graphics driver did not initialize an input driver. Attempting to pick a suitable driver.\n");
      driver.input = tmp;
      if (driver.input != NULL)
      {
         driver.input_data = input_init_func();
         if (driver.input_data == NULL)
         {
            RARCH_ERR("Cannot init input driver. Exiting ...\n");
            rarch_fail(1, "init_video_input()");
         }
      }
      else
      {
         RARCH_ERR("Cannot find input driver. Exiting ...\n");
         rarch_fail(1, "init_video_input()");
      }
   }

#ifdef HAVE_OVERLAY
   if (driver.overlay)
   {
      input_overlay_free(driver.overlay);
      driver.overlay = NULL;
   }

   if (*g_settings.input.overlay)
   {
      driver.overlay = input_overlay_new(g_settings.input.overlay);
      if (!driver.overlay)
         RARCH_ERR("Failed to load overlay.\n");
   }
#endif

   g_extern.measure_data.frame_time_samples_count = 0;
}

void uninit_video_input(void)
{
#ifdef HAVE_OVERLAY
   if (driver.overlay)
   {
      input_overlay_free(driver.overlay);
      driver.overlay = NULL;
      memset(&driver.overlay_state, 0, sizeof(driver.overlay_state));
   }
#endif

   if (driver.input_data != driver.video_data && driver.input)
      input_free_func();

   if (driver.video_data && driver.video)
      video_free_func();

   deinit_pixel_converter();

#ifdef HAVE_DYLIB
   deinit_filter();
#endif

   deinit_shader_dir();
   compute_monitor_fps_statistics();
}

#ifdef HAVE_MENU
static const menu_ctx_driver_t *menu_ctx_drivers[] = {
#if defined(HAVE_RMENU)
   &menu_ctx_rmenu,
#endif
#if defined(HAVE_RMENU_XUI)
   &menu_ctx_rmenu_xui,
#endif
#if defined(HAVE_RGUI)
   &menu_ctx_rgui,
#endif
#if defined(HAVE_LAKKA)
   &menu_ctx_lakka,
#endif
   NULL // zero length array is not valid
};

const void *menu_ctx_find_driver(const char *ident)
{
   unsigned i;
   for (i = 0; menu_ctx_drivers[i]; i++)
   {
      if (strcmp(menu_ctx_drivers[i]->ident, ident) == 0)
         return menu_ctx_drivers[i];
   }

   return NULL;
}

static int find_menu_driver_index(const char *driver)
{
   unsigned i;
   for (i = 0; menu_ctx_drivers[i]; i++)
      if (strcasecmp(driver, menu_ctx_drivers[i]->ident) == 0)
         return i;
   return -1;
}

void find_prev_menu_driver(void)
{
   int i = find_menu_driver_index(g_settings.menu.driver);
   if (i > 0)
   {
      strlcpy(g_settings.menu.driver, menu_ctx_drivers[i - 1]->ident, sizeof(g_settings.menu.driver));
      driver.menu_ctx = (menu_ctx_driver_t*)menu_ctx_drivers[i - 1];
   }
   else
      RARCH_WARN("Couldn't find any previous menu driver (current one: \"%s\").\n", g_settings.menu.driver);
}

void find_next_menu_driver(void)
{
   int i = find_menu_driver_index(g_settings.menu.driver);
   if (i >= 0 && menu_ctx_drivers[i + 1])
   {
      strlcpy(g_settings.menu.driver, menu_ctx_drivers[i + 1]->ident, sizeof(g_settings.menu.driver));
      driver.menu_ctx = (menu_ctx_driver_t*)menu_ctx_drivers[i + 1];
   }
   else
      RARCH_WARN("Couldn't find any next menu driver (current one: \"%s\").\n", g_settings.menu.driver);
}

bool menu_ctx_init_first(const menu_ctx_driver_t **driver, void **data)
{
   unsigned i;

   if (!menu_ctx_drivers[0])
      return false;

   for (i = 0; menu_ctx_drivers[i]; i++)
   {
      void *h = menu_ctx_drivers[i]->init();

      if (h)
      {
         *driver = menu_ctx_drivers[i];
         *data = h;
         strlcpy(g_settings.menu.driver, menu_ctx_drivers[i]->ident, sizeof(g_settings.menu.driver));
         return true;
      }
   }

   return false;
}
#endif

driver_t driver;

