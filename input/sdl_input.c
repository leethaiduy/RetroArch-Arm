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

#include "../driver.h"

#include "SDL.h"
#include "../gfx/gfx_context.h"
#include "../boolean.h"
#include "../general.h"
#include <stdint.h>
#include <stdlib.h>
#include "../libretro.h"
#include "input_common.h"

typedef struct sdl_input
{
   const rarch_joypad_driver_t *joypad;

   int mouse_x, mouse_y;
   int mouse_abs_x, mouse_abs_y;
   int mouse_l, mouse_r, mouse_m;
} sdl_input_t;

static void *sdl_input_init(void)
{
   input_init_keyboard_lut(rarch_key_map_sdl);
   sdl_input_t *sdl = (sdl_input_t*)calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

   sdl->joypad = input_joypad_init_driver(g_settings.input.joypad_driver);
   return sdl;
}

static bool sdl_key_pressed(int key)
{
   if (key >= RETROK_LAST)
      return false;

   int sym = input_translate_rk_to_keysym((enum retro_key)key);

   int num_keys;
   Uint8 *keymap = SDL_GetKeyState(&num_keys);
   if (sym < 0 || sym >= num_keys)
      return false;

   return keymap[sym];
}

static bool sdl_is_pressed(sdl_input_t *sdl, unsigned port_num, const struct retro_keybind *binds, unsigned key)
{
   if (sdl_key_pressed(binds[key].key))
      return true;

   return input_joypad_pressed(sdl->joypad, port_num, binds, key);
}

static int16_t sdl_analog_pressed(sdl_input_t *sdl, const struct retro_keybind *binds,
      unsigned index, unsigned id)
{
   unsigned id_minus = 0;
   unsigned id_plus  = 0;
   input_conv_analog_id_to_bind_id(index, id, &id_minus, &id_plus);

   int16_t pressed_minus = sdl_key_pressed(binds[id_minus].key) ? -0x7fff : 0;
   int16_t pressed_plus = sdl_key_pressed(binds[id_plus].key) ? 0x7fff : 0;
   return pressed_plus + pressed_minus;
}

static bool sdl_bind_button_pressed(void *data, int key)
{
   const struct retro_keybind *binds = g_settings.input.binds[0];
   if (key >= 0 && key < RARCH_BIND_LIST_END)
      return sdl_is_pressed((sdl_input_t*)data, 0, binds, key);
   else
      return false;
}

static int16_t sdl_joypad_device_state(sdl_input_t *sdl, const struct retro_keybind **binds_, 
      unsigned port_num, unsigned id)
{
   const struct retro_keybind *binds = binds_[port_num];
   if (id < RARCH_BIND_LIST_END)
      return binds[id].valid && sdl_is_pressed(sdl, port_num, binds, id);
   else
      return 0;
}

static int16_t sdl_analog_device_state(sdl_input_t *sdl, const struct retro_keybind **binds,
      unsigned port_num, unsigned index, unsigned id)
{
   int16_t ret = sdl_analog_pressed(sdl, binds[port_num], index, id);
   if (!ret)
      ret = input_joypad_analog(sdl->joypad, port_num, index, id, binds[port_num]);
   return ret;
}

static int16_t sdl_keyboard_device_state(sdl_input_t *sdl, unsigned id)
{
   return sdl_key_pressed(id);
}

static int16_t sdl_mouse_device_state(sdl_input_t *sdl, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return sdl->mouse_l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return sdl->mouse_r;
      case RETRO_DEVICE_ID_MOUSE_X:
         return sdl->mouse_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return sdl->mouse_y;
      default:
         return 0;
   }
}

static int16_t sdl_pointer_device_state(sdl_input_t *sdl, unsigned index, unsigned id, bool screen)
{
   if (index != 0)
      return 0;

   int16_t res_x = 0, res_y = 0, res_screen_x = 0, res_screen_y = 0;
   bool valid = input_translate_coord_viewport(sdl->mouse_abs_x, sdl->mouse_abs_y,
         &res_x, &res_y, &res_screen_x, &res_screen_y);

   if (!valid)
      return 0;

   if (screen)
   {
      res_x = res_screen_x;
      res_y = res_screen_y;
   }

   bool inside = (res_x >= -0x7fff) && (res_y >= -0x7fff);

   if (!inside)
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_X:
         return res_x;
      case RETRO_DEVICE_ID_POINTER_Y:
         return res_y;
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         return sdl->mouse_l;
      default:
         return 0;
   }
}

static int16_t sdl_lightgun_device_state(sdl_input_t *sdl, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_X:
         return sdl->mouse_x;
      case RETRO_DEVICE_ID_LIGHTGUN_Y:
         return sdl->mouse_y;
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         return sdl->mouse_l;
      case RETRO_DEVICE_ID_LIGHTGUN_CURSOR:
         return sdl->mouse_m;
      case RETRO_DEVICE_ID_LIGHTGUN_TURBO:
         return sdl->mouse_r;
      case RETRO_DEVICE_ID_LIGHTGUN_START:
         return sdl->mouse_m && sdl->mouse_r; 
      case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
         return sdl->mouse_m && sdl->mouse_l; 
      default:
         return 0;
   }
}

static int16_t sdl_input_state(void *data_, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   sdl_input_t *data = (sdl_input_t*)data_;
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return sdl_joypad_device_state(data, binds, port, id);
      case RETRO_DEVICE_ANALOG:
         return sdl_analog_device_state(data, binds, port, index, id);
      case RETRO_DEVICE_MOUSE:
         return sdl_mouse_device_state(data, id);
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         return sdl_pointer_device_state(data, index, id, device == RARCH_DEVICE_POINTER_SCREEN);
      case RETRO_DEVICE_KEYBOARD:
         return sdl_keyboard_device_state(data, id);
      case RETRO_DEVICE_LIGHTGUN:
         return sdl_lightgun_device_state(data, id);

      default:
         return 0;
   }
}

static void sdl_input_free(void *data)
{
   if (!data)
      return;

   // Flush out all pending events.
   SDL_Event event;
   while (SDL_PollEvent(&event));

   sdl_input_t *sdl = (sdl_input_t*)data;

   if (sdl->joypad)
      sdl->joypad->destroy();

   free(data);
}

static bool sdl_set_rumble(void *data, unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   sdl_input_t *sdl = (sdl_input_t*)data;
   return input_joypad_set_rumble(sdl->joypad, port, effect, strength);
}

static const rarch_joypad_driver_t *sdl_get_joypad_driver(void *data)
{
   sdl_input_t *sdl = (sdl_input_t*)data;
   return sdl->joypad;
}

static void sdl_poll_mouse(sdl_input_t *sdl)
{
   Uint8 btn = SDL_GetRelativeMouseState(&sdl->mouse_x, &sdl->mouse_y);
   SDL_GetMouseState(&sdl->mouse_abs_x, &sdl->mouse_abs_y);
   sdl->mouse_l = SDL_BUTTON(SDL_BUTTON_LEFT) & btn ? 1 : 0;
   sdl->mouse_r = SDL_BUTTON(SDL_BUTTON_RIGHT) & btn ? 1 : 0;
   sdl->mouse_m = SDL_BUTTON(SDL_BUTTON_MIDDLE) & btn ? 1 : 0;
}

static void sdl_input_poll(void *data)
{
   SDL_PumpEvents();
   sdl_input_t *sdl = (sdl_input_t*)data;

   input_joypad_poll(sdl->joypad);
   sdl_poll_mouse(sdl);
}

static uint64_t sdl_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_MOUSE);
   caps |= (1 << RETRO_DEVICE_KEYBOARD);
   caps |= (1 << RETRO_DEVICE_LIGHTGUN);
   caps |= (1 << RETRO_DEVICE_POINTER);
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

const input_driver_t input_sdl = {
   sdl_input_init,
   sdl_input_poll,
   sdl_input_state,
   sdl_bind_button_pressed,
   sdl_input_free,
   NULL,
   NULL,
   NULL,
   sdl_get_capabilities,
   "sdl",
   NULL,
   sdl_set_rumble,
   sdl_get_joypad_driver,
};

