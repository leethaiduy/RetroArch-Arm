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

#include <stdint.h>
#include "../boolean.h"
#include <stddef.h>
#include <string.h>

#include "../file_ext.h"
#include "frontend_salamander.h"
#include "frontend_context.h"


#if defined(__CELLOS_LV2__)
#include "platform/platform_ps3.c"
#elif defined(GEKKO)
#include "platform/platform_gx.c"
#ifdef HW_RVL
#include "platform/platform_wii.c"
#endif
#elif defined(_XBOX)
#include "platform/platform_xdk.c"
#elif defined(PSP)
#include "platform/platform_psp.c"
#endif

default_paths_t default_paths;

//We need to set libretro to the first entry in the cores
//directory so that it will be saved to the config file
static void find_first_libretro_core(char *first_file,
   size_t size_of_first_file, const char *dir,
   const char * ext)
{
   bool ret = false;

   RARCH_LOG("Searching for valid libretro implementation in: \"%s\".\n", dir);

   struct string_list *list = dir_list_new(dir, ext, false);
   if (!list)
   {
      RARCH_ERR("Couldn't read directory. Cannot infer default libretro core.\n");
      return;
   }
   
   for (size_t i = 0; i < list->size && !ret; i++)
   {
      RARCH_LOG("Checking library: \"%s\".\n", list->elems[i].data);
      const char * libretro_elem = list->elems[i].data;

      if (libretro_elem)
      {
         char fname[PATH_MAX];
         fill_pathname_base(fname, libretro_elem, sizeof(fname));

         if (strncmp(fname, SALAMANDER_FILE, sizeof(fname)) == 0)
         {
            if ((i + 1) == list->size)
            {
               RARCH_WARN("Entry is RetroArch Salamander itself, but is last entry. No choice but to set it.\n");
               strlcpy(first_file, fname, size_of_first_file);
            }

            continue;
         }

         strlcpy(first_file, fname, size_of_first_file);
         RARCH_LOG("First found libretro core is: \"%s\".\n", first_file);
         ret = true;
      }
   }

   dir_list_free(list);
}

int main(int argc, char *argv[])
{
   void *args = NULL;
   frontend_ctx_driver_t *frontend_ctx = (frontend_ctx_driver_t*)frontend_ctx_init_first();

   if (!frontend_ctx)
      return 0;

   if (frontend_ctx && frontend_ctx->init)
      frontend_ctx->init(args);

   if (frontend_ctx && frontend_ctx->environment_get)
      frontend_ctx->environment_get(argc, argv, args);

   if (frontend_ctx && frontend_ctx->salamander_init)
      frontend_ctx->salamander_init();

   if (frontend_ctx && frontend_ctx->deinit)
      frontend_ctx->deinit(args);

   if (frontend_ctx && frontend_ctx->exitspawn)
      frontend_ctx->exitspawn();

   return 1;
}
