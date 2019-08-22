/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef RGUI_LIST_H__
#define RGUI_LIST_H__

#ifdef __cplusplus
extern "C" {
#endif

struct rgui_file;
typedef struct rgui_list
{
   struct rgui_file *list;

   size_t capacity;
   size_t size;
} rgui_list_t;

void rgui_list_free(rgui_list_t *list);

void rgui_list_push(void *userdata, const char *path,
      unsigned type, size_t current_directory_ptr);
void rgui_list_pop(rgui_list_t *list, size_t *directory_ptr);
void rgui_list_clear(rgui_list_t *list);

void rgui_list_get_last(const rgui_list_t *list,
      const char **path, unsigned *type);

void rgui_list_get_at_offset(const rgui_list_t *list, size_t index,
      const char **path, unsigned *type);

#ifdef __cplusplus
}
#endif
#endif

