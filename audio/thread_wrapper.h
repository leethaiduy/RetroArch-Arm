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

#ifndef RARCH_AUDIO_THREAD_H__
#define RARCH_AUDIO_THREAD_H__

#include "../driver.h"
#include "../boolean.h"

// Starts a audio driver in a new thread.
// Access to audio driver will be mediated through this driver.
// This driver interfaces with audio callback and is only used in that case.
bool rarch_threaded_audio_init(const audio_driver_t **out_driver, void **out_data,
      const char *device, unsigned out_rate, unsigned latency,
      const audio_driver_t *driver);

#endif

