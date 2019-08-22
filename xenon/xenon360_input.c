/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../driver.h"
#include "../libretro.h"

#include <input/input.h>
#include <usb/usbmain.h>

#define MAX_PADS 4

static uint64_t state[MAX_PADS];

static void xenon360_input_poll(void *data)
{
   (void)data;
   for (unsigned i = 0; i < MAX_PADS; i++)
   {
      struct controller_data_s pad;
      usb_do_poll();
      get_controller_data(&pad, i);

      uint64_t *cur_state = &state[i];

      *cur_state |= pad.b ? RETRO_DEVICE_ID_JOYPAD_A : 0;
      *cur_state |= pad.a ? RETRO_DEVICE_ID_JOYPAD_B : 0;
      *cur_state |= pad.y ? RETRO_DEVICE_ID_JOYPAD_X : 0;
      *cur_state |= pad.x ? RETRO_DEVICE_ID_JOYPAD_Y : 0;
      *cur_state |= pad.left ? RETRO_DEVICE_ID_JOYPAD_LEFT : 0;
      *cur_state |= pad.right ? RETRO_DEVICE_ID_JOYPAD_RIGHT : 0;
      *cur_state |= pad.up ? RETRO_DEVICE_ID_JOYPAD_UP : 0;
      *cur_state |= pad.down ? RETRO_DEVICE_ID_JOYPAD_DOWN : 0;
      *cur_state |= pad.start ? RETRO_DEVICE_ID_JOYPAD_START : 0;
      *cur_state |= pad.back ? RETRO_DEVICE_ID_JOYPAD_SELECT : 0;
      *cur_state |= pad.lt ? RETRO_DEVICE_ID_JOYPAD_L : 0;
      *cur_state |= pad.rt ? RETRO_DEVICE_ID_JOYPAD_R : 0;
   }
}

static int16_t xenon360_input_state(void *data, const struct retro_keybind **binds,
      bool port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;
   (void)index;
   unsigned player = port;
   uint64_t button = binds[player][id].joykey;
   int16_t retval = 0;

   if(player < MAX_PADS)
   {
      switch (device)
      {
         case RETRO_DEVICE_JOYPAD:
            retval = (state[player] & button) ? 1 : 0;
            break;
      }
   }

   return retval;
}

static void xenon360_input_free_input(void *data)
{
   (void)data;
}

static void xenon360_input_set_keybinds(void *data, unsigned device,
      unsigned port, unsigned id, unsigned keybind_action)
{
   (void)data;
   (void)device;
   (void)id;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS))
   {
      for (unsigned i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++)
      {
         g_settings.input.binds[port][i].id = i;
         g_settings.input.binds[port][i].joykey = g_settings.input.binds[port][i].def_joykey;
      }
   }
}

static void* xenon360_input_init(void)
{
   return (void*)-1;
}

static bool xenon360_input_key_pressed(void *data, int key)
{
   return (g_extern.lifecycle_state & (1ULL << key));
}

static uint64_t xenon360_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);

   return caps;
}

const input_driver_t input_xenon360 = {
   xenon360_input_init,
   xenon360_input_poll,
   xenon360_input_state,
   xenon360_input_key_pressed,
   xenon360_input_free_input,
   xenon360_input_set_keybinds,
   NULL,
   NULL,
   xenon360_input_get_capabilities,
   "xenon360",
};
