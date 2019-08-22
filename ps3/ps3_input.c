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

#include <sdk_version.h>
#include "../boolean.h"

#include "sdk_defines.h"

#include "../driver.h"
#include "../libretro.h"
#include "../general.h"

#ifdef HAVE_MOUSE
#ifndef __PSL1GHT__
#define MAX_MICE 7
#endif
#endif

#ifndef __PSL1GHT__
#define MAX_PADS 7
#endif

#define DEADZONE_LOW 105
#define DEADZONE_HIGH 145

typedef struct
{
   float x;
   float y;
   float z;
} sensor_t;

const struct platform_bind platform_keys[] = {
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_B), "Cross button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_Y), "Square button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT), "Select button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_START), "Start button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_UP), "D-Pad Up" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN), "D-Pad Down" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT), "D-Pad Left" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT), "D-Pad Right" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_A), "Circle button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_X), "Triangle button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_L), "L1 button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_R), "R1 button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_L2), "L2 button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_R2), "R2 button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_L3), "L3 button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_R3), "R3 button" },
   { (1ULL << RARCH_TURBO_ENABLE), "Turbo button (unmapped)" },
};

extern const rarch_joypad_driver_t ps3_joypad;

typedef struct ps3_input
{
   uint64_t pad_state[MAX_PADS];
   int16_t analog_state[MAX_PADS][2][2];
   unsigned pads_connected;
#ifdef HAVE_MOUSE
   unsigned mice_connected;
#endif
   sensor_t accelerometer_state[MAX_PADS];
} ps3_input_t;

static void ps3_input_set_keybinds(void *data, unsigned device,
      unsigned port, unsigned id, unsigned keybind_action)
{
   uint64_t *key = &g_settings.input.binds[port][id].joykey;
   size_t arr_size = sizeof(platform_keys) / sizeof(platform_keys[0]);
   (void)device;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BIND))
      *key = g_settings.input.binds[port][id].def_joykey;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS))
   {
      strlcpy(g_settings.input.device_names[port], "DualShock3/Sixaxis", sizeof(g_settings.input.device_names[port]));
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey       = (RETRO_DEVICE_ID_JOYPAD_B);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey       = (RETRO_DEVICE_ID_JOYPAD_Y);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey  = (RETRO_DEVICE_ID_JOYPAD_SELECT);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey   = (RETRO_DEVICE_ID_JOYPAD_START);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey      = (RETRO_DEVICE_ID_JOYPAD_UP);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey    = (RETRO_DEVICE_ID_JOYPAD_DOWN);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey    = (RETRO_DEVICE_ID_JOYPAD_LEFT);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey   = (RETRO_DEVICE_ID_JOYPAD_RIGHT);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey       = (RETRO_DEVICE_ID_JOYPAD_A);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey       = (RETRO_DEVICE_ID_JOYPAD_X);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey       = (RETRO_DEVICE_ID_JOYPAD_L);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey       = (RETRO_DEVICE_ID_JOYPAD_R);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joykey      = (RETRO_DEVICE_ID_JOYPAD_L2);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joykey      = (RETRO_DEVICE_ID_JOYPAD_R2);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey      = (RETRO_DEVICE_ID_JOYPAD_L3);
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey      = (RETRO_DEVICE_ID_JOYPAD_R3);
      g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_PLUS].def_joykey       = NO_BTN;
      g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_MINUS].def_joykey      = NO_BTN;
      g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_PLUS].def_joykey       = NO_BTN;
      g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_MINUS].def_joykey      = NO_BTN;
      g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_PLUS].def_joykey      = NO_BTN;
      g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_MINUS].def_joykey     = NO_BTN;
      g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_PLUS].def_joykey      = NO_BTN;
      g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_MINUS].def_joykey     = NO_BTN;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joyaxis      = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joyaxis      = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joyaxis = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joyaxis  = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joyaxis     = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joyaxis   = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joyaxis   = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joyaxis  = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joyaxis      = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joyaxis      = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joyaxis      = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joyaxis      = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joyaxis     = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joyaxis     = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joyaxis     = AXIS_NONE;
      g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joyaxis     = AXIS_NONE;
      g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_PLUS].def_joyaxis      = AXIS_POS(0);
      g_settings.input.binds[port][RARCH_ANALOG_LEFT_X_MINUS].def_joyaxis     = AXIS_NEG(0);
      g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_PLUS].def_joyaxis      = AXIS_POS(1);
      g_settings.input.binds[port][RARCH_ANALOG_LEFT_Y_MINUS].def_joyaxis     = AXIS_NEG(1);
      g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_PLUS].def_joyaxis     = AXIS_POS(2);
      g_settings.input.binds[port][RARCH_ANALOG_RIGHT_X_MINUS].def_joyaxis    = AXIS_NEG(2);
      g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_PLUS].def_joyaxis     = AXIS_POS(3);
      g_settings.input.binds[port][RARCH_ANALOG_RIGHT_Y_MINUS].def_joyaxis    = AXIS_NEG(3);

      for (int i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++)
      {
         g_settings.input.binds[port][i].id = i;
         g_settings.input.binds[port][i].joykey = g_settings.input.binds[port][i].def_joykey;
         g_settings.input.binds[port][i].joyaxis = g_settings.input.binds[port][i].def_joyaxis;
      }
   }
   
   if (keybind_action & (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL))
   {
      struct platform_bind *ret = (struct platform_bind*)data;

      if (ret->joykey == NO_BTN)
         strlcpy(ret->desc, "No button", sizeof(ret->desc));
      else
      {
         for (size_t i = 0; i < arr_size; i++)
         {
            if (platform_keys[i].joykey == ret->joykey)
            {
               strlcpy(ret->desc, platform_keys[i].desc, sizeof(ret->desc));
               return;
            }
         }
         strlcpy(ret->desc, "Unknown", sizeof(ret->desc));
      }
   }
}

static inline int16_t convert_u8_to_s16(uint8_t val)
{
   if (val == 0)
      return -0x7fff;
   else
      return val * 0x0101 - 0x8000;
}

static void ps3_input_poll(void *data)
{
   CellPadInfo2 pad_info;
   ps3_input_t *ps3 = (ps3_input_t*)data;
   uint64_t *lifecycle_state = &g_extern.lifecycle_state;

   for (unsigned port = 0; port < MAX_PADS; port++)
   {
      static CellPadData state_tmp;
      cellPadGetData(port, &state_tmp);

      if (state_tmp.len != 0)
      {
         uint64_t *state_cur = &ps3->pad_state[port];
         *state_cur = 0;
#ifdef __PSL1GHT__
         *state_cur |= (state_tmp.BTN_LEFT)     ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
         *state_cur |= (state_tmp.BTN_DOWN)     ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
         *state_cur |= (state_tmp.BTN_RIGHT)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
         *state_cur |= (state_tmp.BTN_UP)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
         *state_cur |= (state_tmp.BTN_START)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_START) : 0;
         *state_cur |= (state_tmp.BTN_R3)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
         *state_cur |= (state_tmp.BTN_L3)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
         *state_cur |= (state_tmp.BTN_SELECT)   ? (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
         *state_cur |= (state_tmp.BTN_TRIANGLE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0;
         *state_cur |= (state_tmp.BTN_SQUARE)   ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
         *state_cur |= (state_tmp.BTN_CROSS)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0;
         *state_cur |= (state_tmp.BTN_CIRCLE)   ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0;
         *state_cur |= (state_tmp.BTN_R1)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0;
         *state_cur |= (state_tmp.BTN_L1)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0;
         *state_cur |= (state_tmp.BTN_R2)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
         *state_cur |= (state_tmp.BTN_L2)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
#else
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_LEFT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_DOWN) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_RIGHT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_UP) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_START) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_START) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_R3) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_L3) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_SELECT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_TRIANGLE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_SQUARE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0;

         if (*lifecycle_state & (1ULL << MODE_MENU))
         {
            int value = 0;
            if (cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_ENTER_BUTTON_ASSIGN, &value) == 0)
            {
               if (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CROSS)
                  *state_cur |=  (value == CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CROSS) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : (1ULL << RETRO_DEVICE_ID_JOYPAD_B);
               if (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CIRCLE)
                  *state_cur |=  (value == CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CIRCLE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : (1ULL << RETRO_DEVICE_ID_JOYPAD_B);
            }
         }
         else
         {
            *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CROSS) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0;
            *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CIRCLE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0;
         }
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R1) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L1) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
         //RARCH_LOG("lsx : %d (%hd) lsy : %d (%hd) rsx : %d (%hd) rsy : %d (%hd)\n", lsx, ls_x, lsy, ls_y, rsx, rs_x, rsy, rs_y);
         uint8_t lsx = (uint8_t)(state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X]);
         uint8_t lsy = (uint8_t)(state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y]);
         uint8_t rsx = (uint8_t)(state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X]);
         uint8_t rsy = (uint8_t)(state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y]);
#if 0
         if (!(lsx < DEADZONE_LOW || DEADZONE_HIGH < lsx))
            lsx = 128;
         if (!(lsy < DEADZONE_LOW || DEADZONE_HIGH < lsy))
            lsy = 128;
         if (!(rsx < DEADZONE_LOW || DEADZONE_HIGH < rsx))
            rsx = 128;
         if (!(rsy < DEADZONE_LOW || DEADZONE_HIGH < rsy))
            rsy = 128;
#endif
         ps3->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT ][RETRO_DEVICE_ID_ANALOG_X] = convert_u8_to_s16(lsx);
         ps3->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT ][RETRO_DEVICE_ID_ANALOG_Y] = convert_u8_to_s16(lsy);
         ps3->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = convert_u8_to_s16(rsx);
         ps3->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = convert_u8_to_s16(rsy);

         ps3->accelerometer_state[port].x = state_tmp.button[CELL_PAD_BTN_OFFSET_SENSOR_X];
         ps3->accelerometer_state[port].y = state_tmp.button[CELL_PAD_BTN_OFFSET_SENSOR_Y];
         ps3->accelerometer_state[port].z = state_tmp.button[CELL_PAD_BTN_OFFSET_SENSOR_Z];
#endif
         if (g_settings.input.autodetect_enable)
         {
            if (strcmp(g_settings.input.device_names[port], "DualShock3/Sixaxis") != 0)
               ps3_input_set_keybinds(NULL, DEVICE_SIXAXIS, port, 0, (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS));
         }
      }

      for (int i = 0; i < 2; i++)
         for (int j = 0; j < 2; j++)
            if (ps3->analog_state[port][i][j] == -0x8000)
               ps3->analog_state[port][i][j] = -0x7fff;
   }

   uint64_t *state_p1 = &ps3->pad_state[0];

   *lifecycle_state &= ~((1ULL << RARCH_MENU_TOGGLE));

   if ((*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_L3)) && (*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_R3)))
      *lifecycle_state |= (1ULL << RARCH_MENU_TOGGLE);

   cellPadGetInfo2(&pad_info);
   ps3->pads_connected = pad_info.now_connect; 

#ifdef HAVE_MOUSE
   CellMouseInfo mouse_info;
   cellMouseGetInfo(&mouse_info);
   ps3->mice_connected = mouse_info.now_connect;
#endif
}

#ifdef HAVE_MOUSE

static int16_t ps3_mouse_device_state(void *data, unsigned player, unsigned id)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;
   CellMouseData mouse_state;
   cellMouseGetData(id, &mouse_state);

   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return (!ps3->mice_connected ? 0 : mouse_state.buttons & CELL_MOUSE_BUTTON_1);
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return (!ps3->mice_connected ? 0 : mouse_state.buttons & CELL_MOUSE_BUTTON_2);
      case RETRO_DEVICE_ID_MOUSE_X:
         return (!ps3->mice_connected ? 0 : mouse_state.x_axis);
      case RETRO_DEVICE_ID_MOUSE_Y:
         return (!ps3->mice_connected ? 0 : mouse_state.y_axis);
      default:
         return 0;
   }
}

#endif

static bool ps3_menu_input_state(uint64_t joykey, uint64_t state)
{
   switch (joykey)
   {
      case CONSOLE_MENU_A:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_A);
      case CONSOLE_MENU_B:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_B);
      case CONSOLE_MENU_X:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_X);
      case CONSOLE_MENU_Y:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_Y);
      case CONSOLE_MENU_START:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_START);
      case CONSOLE_MENU_SELECT:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT);
      case CONSOLE_MENU_UP:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_UP);
      case CONSOLE_MENU_DOWN:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN);
      case CONSOLE_MENU_LEFT:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT);
      case CONSOLE_MENU_RIGHT:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT);
      case CONSOLE_MENU_L:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_L);
      case CONSOLE_MENU_R:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_R);
      case CONSOLE_MENU_HOME:
         return (state & (1ULL << RETRO_DEVICE_ID_JOYPAD_L3)) && (state & (1ULL << RETRO_DEVICE_ID_JOYPAD_R3));
      case CONSOLE_MENU_L2:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_L2);
      case CONSOLE_MENU_R2:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_R2);
      case CONSOLE_MENU_L3:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_L3);
      case CONSOLE_MENU_R3:
         return state & (1ULL << RETRO_DEVICE_ID_JOYPAD_R3);
      default:
         return false;
   }
}

static int16_t ps3_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;
   int16_t retval = 0;

   if (port < ps3->pads_connected)
   {
      switch (device)
      {
         case RETRO_DEVICE_JOYPAD:
            if (binds[port][id].joykey >= CONSOLE_MENU_FIRST && binds[port][id].joykey <= CONSOLE_MENU_LAST)
               return ps3_menu_input_state(binds[port][id].joykey, ps3->pad_state[port]) ? 1 : 0;
            else
               return input_joypad_pressed(&ps3_joypad, port, binds[port], id);
         case RETRO_DEVICE_ANALOG:
            return input_joypad_analog(&ps3_joypad, port, index, id, binds[port]);
#if 0
         case RETRO_DEVICE_SENSOR_ACCELEROMETER:
            switch (id)
            {
               // fixed range of 0x000 - 0x3ff
               case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_X:
                  retval = ps3->accelerometer_state[port].x;
                  break;
               case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_Y:
                  retval = ps3->accelerometer_state[port].y;
                  break;
               case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_Z:
                  retval = ps3->accelerometer_state[port].z;
                  break;
               default:
                  retval = 0;
            }
            break;
#endif
#ifdef HAVE_MOUSE
         case RETRO_DEVICE_MOUSE:
            retval = ps3_mouse_device_state(data, port, id);
            break;
#endif
         default:
            return 0;
      }
   }

   return retval;
}

static void ps3_input_free_input(void *data)
{
   if (!data)
      return;

#ifndef __CELLOS_LV2__
   cellPadEnd();
#endif
#ifdef HAVE_MOUSE
   //cellMouseEnd();
#endif
}


static void* ps3_input_init(void)
{
   ps3_input_t *ps3 = (ps3_input_t*)calloc(1, sizeof(*ps3));
   if (!ps3)
      return NULL;

   cellPadInit(MAX_PADS);
#ifdef HAVE_MOUSE
   cellMouseInit(MAX_MICE);
#endif

   return ps3;
}

static bool ps3_input_key_pressed(void *data, int key)
{
   return (g_extern.lifecycle_state & (1ULL << key)) || input_joypad_pressed(&ps3_joypad, 0, g_settings.input.binds[0], key);
}

static uint64_t ps3_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
#ifdef HAVE_MOUSE
   caps |= (1 << RETRO_DEVICE_MOUSE);
#endif
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

static bool ps3_input_set_sensor_state(void *data, unsigned port, enum retro_sensor_action action, unsigned event_rate)
{
   CellPadInfo2 pad_info;
   (void)event_rate;

   switch (action)
   {
      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
         cellPadGetInfo2(&pad_info);
         if ((pad_info.device_capability[port] & CELL_PAD_CAPABILITY_SENSOR_MODE) != CELL_PAD_CAPABILITY_SENSOR_MODE)
            return false;

         cellPadSetPortSetting(port, CELL_PAD_SETTING_SENSOR_ON);
         return true;
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         cellPadSetPortSetting(port, 0);
         return true;

      default:
         return false;
   }
}

static bool ps3_input_set_rumble(void *data, unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   CellPadActParam params;

   switch (effect)
   {
      case RETRO_RUMBLE_WEAK:
         if (strength > 1)
            strength = 1;
         params.motor[0] = strength;
         break;
      case RETRO_RUMBLE_STRONG:
         if (strength > 255)
            strength = 255;
         params.motor[1] = strength;
   }

   cellPadSetActDirect(port, &params);

   return true;
}

static const rarch_joypad_driver_t *ps3_input_get_joypad_driver(void *data)
{
   return &ps3_joypad;
}

const input_driver_t input_ps3 = {
   ps3_input_init,
   ps3_input_poll,
   ps3_input_state,
   ps3_input_key_pressed,
   ps3_input_free_input,
   ps3_input_set_keybinds,
   ps3_input_set_sensor_state,
   NULL,
   ps3_input_get_capabilities,
   "ps3",

   NULL,
   ps3_input_set_rumble,
   ps3_input_get_joypad_driver,
};

static bool ps3_joypad_init(void)
{
   return true;
}

static bool ps3_joypad_button(unsigned port_num, uint16_t joykey)
{
   ps3_input_t *ps3 = (ps3_input_t*)driver.input_data;

   if (port_num >= MAX_PADS)
      return false;

   return ps3->pad_state[port_num] & (1ULL << joykey);
}

static int16_t ps3_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   ps3_input_t *ps3 = (ps3_input_t*)driver.input_data;
   if (joyaxis == AXIS_NONE || port_num >= MAX_PADS)
      return 0;

   int val = 0;

   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      axis = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   switch (axis)
   {
      case 0: val = ps3->analog_state[port_num][0][0]; break;
      case 1: val = ps3->analog_state[port_num][0][1]; break;
      case 2: val = ps3->analog_state[port_num][1][0]; break;
      case 3: val = ps3->analog_state[port_num][1][1]; break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static void ps3_joypad_poll(void)
{
}

static bool ps3_joypad_query_pad(unsigned pad)
{
   ps3_input_t *ps3 = (ps3_input_t*)driver.input_data;
   return pad < MAX_PLAYERS && ps3->pad_state[pad];
}

static const char *ps3_joypad_name(unsigned pad)
{
   return NULL;
}

static void ps3_joypad_destroy(void)
{
}

const rarch_joypad_driver_t ps3_joypad = {
   ps3_joypad_init,
   ps3_joypad_query_pad,
   ps3_joypad_destroy,
   ps3_joypad_button,
   ps3_joypad_axis,
   ps3_joypad_poll,
   NULL,
   ps3_joypad_name,
   "ps3",
};
