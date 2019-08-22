/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#include <string.h>

#import "RetroArch_Apple.h"
#include "rarch_wrapper.h"
#include "../../frontend/frontend.h"

#include "apple_input.h"

#include "../../file.h"

id<RetroArch_Platform> apple_platform;

#pragma mark EMULATION
bool apple_is_running;
bool apple_use_tv_mode;
NSString* apple_core;

static CFRunLoopObserverRef iterate_observer;

static void apple_rarch_exited()
{
   NSString* used_core = apple_core;
   apple_core = 0;
   
   if (apple_is_running)
   {
      apple_is_running = false;
      [apple_platform unloadingCore:used_core];
   }
   
#ifdef OSX
   [used_core release];
#endif
   
   if (apple_use_tv_mode)
      apple_run_core(nil, 0);
}

static void do_iteration()
{
    bool iterate = iterate_observer && apple_is_running && !g_extern.is_paused;
    
    if (!iterate)
        return;
    
    if (main_entry_iterate(0, NULL, NULL))
    {
        main_exit(NULL);
        apple_rarch_exited();
    }
    else
        CFRunLoopWakeUp(CFRunLoopGetMain());
}

void apple_start_iteration()
{
   if (iterate_observer)
       return;
    
    iterate_observer = CFRunLoopObserverCreate(0, kCFRunLoopBeforeWaiting, true, 0, do_iteration, 0);
    CFRunLoopAddObserver(CFRunLoopGetMain(), iterate_observer, kCFRunLoopCommonModes);
}

void apple_stop_iteration()
{
   if (!iterate_observer)
       return;
    
    CFRunLoopObserverInvalidate(iterate_observer);
    CFRelease(iterate_observer);
    iterate_observer = 0;
}

void apple_run_core(NSString* core, const char* file)
{
   if (apple_is_running)
       return;
    
    [apple_platform loadingCore:core withFile:file];
    
    apple_core = core;
    apple_is_running = true;

   static char core_path[PATH_MAX];
   static char file_path[PATH_MAX];

   if (file && core)
   {
      strlcpy(core_path, apple_core.UTF8String, sizeof(core_path));
      strlcpy(file_path, file, sizeof(file_path));
   }
   
#ifdef IOS
    static char config_path[PATH_MAX];
   
    if (apple_core_info_has_custom_config(apple_core.UTF8String))
        apple_core_info_get_custom_config(apple_core.UTF8String, config_path, sizeof(config_path));
    else
        strlcpy(config_path, apple_platform.globalConfigFile.UTF8String, sizeof(config_path));
    
    static const char* const argv_game[] = { "retroarch", "-c", config_path, "-L", core_path, file_path, 0 };
    static const char* const argv_menu[] = { "retroarch", "-c", config_path, "--menu", 0 };
    
    int argc = (file && core) ? 6 : 4;
    char** argv = (char**)((file && core) ? argv_game : argv_menu);
#else
   static const char* const argv_game[] = { "retroarch", "-L", core_path, file_path, 0 };
   static const char* const argv_menu[] = { "retroarch", "--menu", 0 };

   int argc = (file && core) ? 4 : 2;
   char** argv = (char**)((file && core) ? argv_game : argv_menu);
#endif

    if (apple_rarch_load_content(argc, argv))
    {
        char basedir[256];
        fill_pathname_basedir(basedir, file ? file : "", sizeof(basedir));
        if (file && access(basedir, R_OK | W_OK | X_OK))
            apple_display_alert(BOXSTRING("The directory containing the selected file must have write premissions. This will prevent zipped content from loading, and will cause some cores to not function."), 0);
        else
            apple_display_alert(BOXSTRING("Failed to load content."), 0);
        
        apple_rarch_exited();
    }
}
