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

#ifndef __DYNAMIC_H
#define __DYNAMIC_H

#include "boolean.h"
#include "libretro.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
#define NEED_DYNAMIC
#else
#undef NEED_DYNAMIC
#endif

#ifdef __cplusplus
extern "C" {
#endif

void init_libretro_sym(bool dummy);
void uninit_libretro_sym(void);

typedef void *dylib_t;
#ifdef NEED_DYNAMIC
typedef void (*function_t)(void);

dylib_t dylib_load(const char *path);
void dylib_close(dylib_t lib);
function_t dylib_proc(dylib_t lib, const char *proc);
#endif

// Sets environment callback in order to get statically known information from it.
// Fetched via environment callbacks instead of retro_get_system_info(), as this info
// is part of extensions.
// Should only be called once right after core load to avoid overwriting
// the "real" environ callback.
//
// For statically linked cores, pass retro_set_environment as argument.
void libretro_get_environment_info(void (*)(retro_environment_t), bool *load_no_rom);

#ifdef HAVE_DYNAMIC
// Gets system info from an arbitrary lib.
// The struct returned must be freed as strings are allocated dynamically.
bool libretro_get_system_info(const char *path, struct retro_system_info *info, bool *load_no_rom);
void libretro_free_system_info(struct retro_system_info *info);
#endif

// Transforms a library id to a name suitable as a pathname.
void libretro_get_current_core_pathname(char *name, size_t size);

const struct retro_subsystem_info *libretro_find_subsystem_info(const struct retro_subsystem_info *info, unsigned num_info, const char *ident);
const struct retro_controller_description *libretro_find_controller_description(const struct retro_controller_info *info, unsigned id);

extern void (*pretro_init)(void);
extern void (*pretro_deinit)(void);

extern unsigned (*pretro_api_version)(void);

extern void (*pretro_get_system_info)(struct retro_system_info*);
extern void (*pretro_get_system_av_info)(struct retro_system_av_info*);

extern void (*pretro_set_environment)(retro_environment_t);
extern void (*pretro_set_video_refresh)(retro_video_refresh_t);
extern void (*pretro_set_audio_sample)(retro_audio_sample_t);
extern void (*pretro_set_audio_sample_batch)(retro_audio_sample_batch_t);
extern void (*pretro_set_input_poll)(retro_input_poll_t);
extern void (*pretro_set_input_state)(retro_input_state_t);

extern void (*pretro_set_controller_port_device)(unsigned, unsigned);

extern void (*pretro_reset)(void);
extern void (*pretro_run)(void);

extern size_t (*pretro_serialize_size)(void);
extern bool (*pretro_serialize)(void*, size_t);
extern bool (*pretro_unserialize)(const void*, size_t);

extern void (*pretro_cheat_reset)(void);
extern void (*pretro_cheat_set)(unsigned, bool, const char*);

extern bool (*pretro_load_game)(const struct retro_game_info*);
extern bool (*pretro_load_game_special)(unsigned, const struct retro_game_info*, size_t);

extern void (*pretro_unload_game)(void);

extern unsigned (*pretro_get_region)(void);

extern void *(*pretro_get_memory_data)(unsigned);
extern size_t (*pretro_get_memory_size)(unsigned);

extern bool rarch_environment_cb(unsigned cmd, void *data);

#ifdef __cplusplus
}
#endif

#endif

