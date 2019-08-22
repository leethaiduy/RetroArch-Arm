/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2014 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <bps/bps.h>

#include <stdint.h>
#include "../../boolean.h"
#include <stddef.h>
#include <string.h>
#include "../../dynamic.h"
#include "../../libretro_private.h"

static void get_environment_settings(int argc, char *argv[], void *args)
{
   (void)argc;
   (void)argv;

/* FIXME - should this apply for both BB10 and PB? */
#if defined(__QNX__) && !defined(HAVE_BB10)
   rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, (void*)"app/native/lib");
#endif

   config_load();
}

static void system_init(void *data)
{
   (void)data;
/* FIXME - should this apply for both BB10 and PB? */
#if defined(__QNX__) && !defined(HAVE_BB10)
   bps_initialize();
#endif
}

static void system_shutdown(void)
{
   bps_shutdown();
}

const frontend_ctx_driver_t frontend_ctx_qnx = {
   get_environment_settings,     /* get_environment_settings */
   system_init,                  /* init */
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* process_events */
   NULL,                         /* exec */
   system_shutdown,              /* shutdown */
   "qnx",
};
