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

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspfpu.h>
#include <psppower.h>

#include <stdint.h>
#include "../../boolean.h"
#include <stddef.h>
#include <string.h>

#include "../../psp/sdk_defines.h"

PSP_MODULE_INFO("RetroArch PSP", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER|THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_MAX();

static int exit_callback(int arg1, int arg2, void *common)
{
   g_extern.verbose = false;

#ifdef HAVE_FILE_LOGGER
   if (g_extern.log_file)
      fclose(g_extern.log_file);
   g_extern.log_file = NULL;
#endif

   sceKernelExitGame();
   return 0;
}

static void get_environment_settings(int argc, char *argv[], void *args)
{
   (void)args;
#ifndef IS_SALAMANDER
   g_extern.verbose = true;

#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   g_extern.log_file = fopen("ms0:/retroarch-log.txt", "w");
#endif
#endif

   fill_pathname_basedir(default_paths.port_dir, argv[0], sizeof(default_paths.port_dir));
   RARCH_LOG("port dir: [%s]\n", default_paths.port_dir);

   fill_pathname_join(default_paths.core_dir, default_paths.port_dir, "cores", sizeof(default_paths.core_dir));
   fill_pathname_join(default_paths.savestate_dir, default_paths.core_dir, "savestates", sizeof(default_paths.savestate_dir));
   fill_pathname_join(default_paths.sram_dir, default_paths.core_dir, "savefiles", sizeof(default_paths.sram_dir));
   fill_pathname_join(default_paths.system_dir, default_paths.core_dir, "system", sizeof(default_paths.system_dir));

   /* now we fill in all the variables */
   fill_pathname_join(g_extern.config_path, default_paths.port_dir, "retroarch.cfg", sizeof(g_extern.config_path));
}

int callback_thread(SceSize args, void *argp)
{
   int cbid = sceKernelCreateCallback("Exit callback", exit_callback, NULL);
   sceKernelRegisterExitCallback(cbid);
   sceKernelSleepThreadCB();

   return 0;
}

static int setup_callback(void)
{
   int thread_id = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);

   if (thread_id >= 0)
      sceKernelStartThread(thread_id, 0, 0);

   return thread_id;
}

static void system_init(void *data)
{
   (void)data;
   //initialize debug screen
   pspDebugScreenInit(); 
   pspDebugScreenClear();
   
   setup_callback();
   
   pspFpuSetEnable(0);//disable FPU exceptions
   scePowerSetClockFrequency(333,333,166);
}

static void system_deinit(void *data)
{
   (void)data;
   sceKernelExitGame();
}

static int psp_process_args(int argc, char *argv[], void *args)
{
   (void)argc;
   (void)args;

   if (argv[1] && (argv[1][0]))
   {
      strlcpy(g_extern.fullpath, argv[1], sizeof(g_extern.fullpath));
      g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
      return 1;
   }

   return 0;
}

const frontend_ctx_driver_t frontend_ctx_psp = {
   get_environment_settings,     /* get_environment_settings */
   system_init,                  /* init */
   system_deinit,                /* deinit */
   NULL,                         /* exitspawn */
   psp_process_args,             /* process_args */
   NULL,                         /* process_events */
   NULL,                  	      /* exec */
   NULL,                         /* shutdown */
   "psp",
#ifdef IS_SALAMANDER
   NULL,
#endif
};
