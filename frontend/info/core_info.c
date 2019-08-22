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

#include "core_info.h"
#include "../../general.h"
#include "../../file.h"
#include "../../file_ext.h"
#include "../../file_extract.h"
#include "../../config.def.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

static void core_info_list_resolve_all_extensions(core_info_list_t *core_info_list)
{
   size_t i, all_ext_len = 0;
   for (i = 0; i < core_info_list->count; i++)
   {
      all_ext_len += core_info_list->list[i].supported_extensions ?
         (strlen(core_info_list->list[i].supported_extensions) + 2) : 0;
   }

   if (all_ext_len)
   {
      all_ext_len += strlen("|zip");
      core_info_list->all_ext = (char*)calloc(1, all_ext_len);
   }

   if (core_info_list->all_ext)
   {
      for (i = 0; i < core_info_list->count; i++)
      {
         if (core_info_list->list[i].supported_extensions)
         {
            strlcat(core_info_list->all_ext, core_info_list->list[i].supported_extensions, all_ext_len);
            strlcat(core_info_list->all_ext, "|", all_ext_len);
         }
      }
      strlcat(core_info_list->all_ext, "|zip", all_ext_len);
   }
}

static void core_info_list_resolve_all_firmware(core_info_list_t *core_info_list)
{
   size_t i;
   unsigned c;

   for (i = 0; i < core_info_list->count; i++)
   {
      core_info_t *info = &core_info_list->list[i];

      if (!info->data)
         continue;

      unsigned count = 0;
      if (!config_get_uint(info->data, "firmware_count", &count))
         continue;

      info->firmware = (core_info_firmware_t*)calloc(count, sizeof(*info->firmware));
      if (!info->firmware)
         continue;

      for (c = 0; c < count; c++)
      {
         char path_key[64], desc_key[64], opt_key[64];

         snprintf(path_key, sizeof(path_key), "firmware%u_path", c);
         snprintf(desc_key, sizeof(desc_key), "firmware%u_desc", c);
         snprintf(opt_key, sizeof(opt_key), "firmware%u_opt", c);

         config_get_string(info->data, path_key, &info->firmware[c].path);
         config_get_string(info->data, desc_key, &info->firmware[c].desc);
         config_get_bool(info->data, opt_key , &info->firmware[c].optional);
      }
   }
}

core_info_list_t *core_info_list_new(const char *modules_path)
{
   struct string_list *contents = dir_list_new(modules_path, EXT_EXECUTABLES, false);
   size_t i;

   core_info_t *core_info = NULL;
   core_info_list_t *core_info_list = NULL;

   if (!contents)
      return NULL;

   core_info_list = (core_info_list_t*)calloc(1, sizeof(*core_info_list));
   if (!core_info_list)
      goto error;

   core_info = (core_info_t*)calloc(contents->size, sizeof(*core_info));
   if (!core_info)
      goto error;

   core_info_list->list = core_info;
   core_info_list->count = contents->size;

   for (i = 0; i < contents->size; i++)
   {
      char info_path_base[PATH_MAX], info_path[PATH_MAX];
      core_info[i].path = strdup(contents->elems[i].data);

      if (!core_info[i].path)
         break;

      fill_pathname_base(info_path_base, contents->elems[i].data, sizeof(info_path_base));
      path_remove_extension(info_path_base);

#if defined(RARCH_MOBILE) || defined(RARCH_CONSOLE)
      char *substr = strrchr(info_path_base, '_');
      if (substr)
         *substr = '\0';
#endif

      strlcat(info_path_base, ".info", sizeof(info_path_base));

      fill_pathname_join(info_path, (*g_settings.libretro_info_path) ? g_settings.libretro_info_path : modules_path,
            info_path_base, sizeof(info_path));

      core_info[i].data = config_file_new(info_path);

      if (core_info[i].data)
      {
         unsigned count = 0;
         config_get_string(core_info[i].data, "display_name", &core_info[i].display_name);
         config_get_uint(core_info[i].data, "firmware_count", &count);
         core_info[i].firmware_count = count;
         if (config_get_string(core_info[i].data, "supported_extensions", &core_info[i].supported_extensions) &&
               core_info[i].supported_extensions)
            core_info[i].supported_extensions_list = string_split(core_info[i].supported_extensions, "|");

         if (config_get_string(core_info[i].data, "authors", &core_info[i].authors) &&
               core_info[i].authors)
            core_info[i].authors_list = string_split(core_info[i].authors, "|");

         if (config_get_string(core_info[i].data, "permissions", &core_info[i].permissions) &&
               core_info[i].permissions)
            core_info[i].permissions_list = string_split(core_info[i].permissions, "|");
         if (config_get_string(core_info[i].data, "notes", &core_info[i].notes) &&
               core_info[i].notes)
            core_info[i].note_list = string_split(core_info[i].notes, "|");
      }

      if (!core_info[i].display_name)
         core_info[i].display_name = strdup(path_basename(core_info[i].path));
   }

   core_info_list_resolve_all_extensions(core_info_list);
   core_info_list_resolve_all_firmware(core_info_list);

   dir_list_free(contents);
   return core_info_list;

error:
   if (contents)
      dir_list_free(contents);
   core_info_list_free(core_info_list);
   return NULL;
}

void core_info_list_free(core_info_list_t *core_info_list)
{
   size_t i, j;
   if (!core_info_list)
      return;

   for (i = 0; i < core_info_list->count; i++)
   {
      core_info_t *info = &core_info_list->list[i];

      free(info->path);
      free(info->display_name);
      free(info->supported_extensions);
      free(info->authors);
      free(info->permissions);
      free(info->notes);
      string_list_free(info->supported_extensions_list);
      string_list_free(info->authors_list);
      string_list_free(info->note_list);
      string_list_free(info->permissions_list);
      config_file_free(info->data);

      for (j = 0; j < info->firmware_count; j++)
      {
         free(info->firmware[j].path);
         free(info->firmware[j].desc);
      }
      free(info->firmware);
   }

   free(core_info_list->all_ext);
   free(core_info_list->list);
   free(core_info_list);
}

size_t core_info_list_num_info_files(core_info_list_t *core_info_list)
{
   size_t i, num;
   num = 0;
   for (i = 0; i < core_info_list->count; i++)
      num += !!core_info_list->list[i].data;
   return num;
}

bool core_info_list_get_display_name(core_info_list_t *core_info_list, const char *path, char *buf, size_t size)
{
   size_t i;
   for (i = 0; i < core_info_list->count; i++)
   {
      const core_info_t *info = &core_info_list->list[i];
      if (!strcmp(path_basename(info->path), path_basename(path)) && info->display_name)
      {
         strlcpy(buf, info->display_name, size);
         return true;
      }
   }

   return false;
}

bool core_info_list_get_info(core_info_list_t *core_info_list, core_info_t *out_info, const char *path)
{
   size_t i;
   memset(out_info, 0, sizeof(*out_info));

   for (i = 0; i < core_info_list->count; i++)
   {
      const core_info_t *info = &core_info_list->list[i];
      if (!strcmp(path_basename(info->path), path_basename(path)))
      {
         *out_info = *info;
         return true;
      }
   }

   return false;
}

bool core_info_does_support_any_file(const core_info_t *core, const struct string_list *list)
{
   size_t i;
   if (!list || !core || !core->supported_extensions_list)
      return false;

   for (i = 0; i < list->size; i++)
      if (string_list_find_elem_prefix(core->supported_extensions_list, ".", path_get_extension(list->elems[i].data)))
         return true;
   return false;
}

bool core_info_does_support_file(const core_info_t *core, const char *path)
{
   if (!path || !core || !core->supported_extensions_list)
      return false;

   return string_list_find_elem_prefix(core->supported_extensions_list, ".", path_get_extension(path));
}

const char *core_info_list_get_all_extensions(core_info_list_t *core_info_list)
{
   return core_info_list->all_ext;
}

// qsort_r() is not in standard C, sadly.
static const char *core_info_tmp_path;
static const struct string_list *core_info_tmp_list;

static int core_info_qsort_cmp(const void *a_, const void *b_)
{
   const core_info_t *a = (const core_info_t*)a_;
   const core_info_t *b = (const core_info_t*)b_;

   int support_a = core_info_does_support_any_file(a, core_info_tmp_list) ||
      core_info_does_support_file(a, core_info_tmp_path);
   int support_b = core_info_does_support_any_file(b, core_info_tmp_list) ||
      core_info_does_support_file(b, core_info_tmp_path);

   if (support_a != support_b)
      return support_b - support_a;
   else
      return strcasecmp(a->display_name, b->display_name);
}

void core_info_list_get_supported_cores(core_info_list_t *core_info_list, const char *path,
      const core_info_t **infos, size_t *num_infos)
{
   core_info_tmp_path = path;

#ifdef HAVE_ZLIB
   struct string_list *list = NULL;
   if (!strcasecmp(path_get_extension(path), "zip"))
      list = zlib_get_file_list(path);
   core_info_tmp_list = list;
#endif

   // Let supported core come first in list so we can return a pointer to them.
   qsort(core_info_list->list, core_info_list->count, sizeof(core_info_t), core_info_qsort_cmp);

   size_t supported, i;
   supported = 0;
   for (i = 0; i < core_info_list->count; i++, supported++)
   {
      const core_info_t *core = &core_info_list->list[i];
      if (!core_info_does_support_file(core, path)
#ifdef HAVE_ZLIB
            && !core_info_does_support_any_file(core, list)
#endif
         )
         break;
   }

#ifdef HAVE_ZLIB
   if (list)
      string_list_free(list);
#endif

   *infos = core_info_list->list;
   *num_infos = supported;
}

static core_info_t *find_core_info(core_info_list_t *list, const char *core)
{
   size_t i;
   for (i = 0; i < list->count; i++)
   {
      core_info_t *info = &list->list[i];
      if (info->path && !strcmp(info->path, core))
         return info;
   }

   return NULL;
}

static int core_info_firmware_cmp(const void *a_, const void *b_)
{
   const core_info_firmware_t *a = (const core_info_firmware_t*)a_;
   const core_info_firmware_t *b = (const core_info_firmware_t*)b_;
   int order = b->missing - a->missing;
   if (order)
      return order;
   else
      return strcasecmp(a->path, b->path);
}

void core_info_list_update_missing_firmware(core_info_list_t *core_info_list,
      const char *core, const char *systemdir)
{
   size_t i;
   char path[PATH_MAX];

   core_info_t *info = find_core_info(core_info_list, core);
   if (!info)
      return;

   for (i = 0; i < info->firmware_count; i++)
   {
      if (info->firmware[i].path)
      {
         fill_pathname_join(path, systemdir, info->firmware[i].path, sizeof(path));
         info->firmware[i].missing = !path_file_exists(path);
      }
   }
}

void core_info_list_get_missing_firmware(core_info_list_t *core_info_list,
      const char *core, const char *systemdir,
      const core_info_firmware_t **firmware, size_t *num_firmware)
{
   size_t i;
   char path[PATH_MAX];

   *firmware = NULL;
   *num_firmware = 0;

   core_info_t *info = find_core_info(core_info_list, core);
   if (!info)
      return;

   *firmware = info->firmware;

   for (i = 1; i < info->firmware_count; i++)
   {
      fill_pathname_join(path, systemdir, info->firmware[i].path, sizeof(path));
      info->firmware[i].missing = !path_file_exists(path);
      *num_firmware += info->firmware[i].missing;
   }

   qsort(info->firmware, info->firmware_count, sizeof(*info->firmware), core_info_firmware_cmp);
}
