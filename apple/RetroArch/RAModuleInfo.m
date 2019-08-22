/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#import "RetroArch_Apple.h"
#import "RAModuleInfo.h"

#include "file.h"
#include "core_info.h"

static NSMutableArray* moduleList;
static core_info_list_t* coreList;

@implementation RAModuleInfo
+ (NSArray*)getModules
{
   if (!moduleList)
   {   
      coreList = get_core_info_list(apple_platform.corePath.UTF8String);
      
      if (!coreList)
         return nil;
      
      moduleList = [NSMutableArray arrayWithCapacity:coreList->count];

      for (int i = 0; coreList && i < coreList->count; i ++)
      {
         core_info_t* core = &coreList->list[i];
      
         RAModuleInfo* newInfo = [RAModuleInfo new];
         newInfo.path = [NSString stringWithUTF8String:core->path];
         newInfo.info = core;
         newInfo.data = core->data;
         newInfo.description = [NSString stringWithUTF8String:core->display_name];

         [moduleList addObject:newInfo];
      }
      
      [moduleList sortUsingComparator:^(RAModuleInfo* left, RAModuleInfo* right)
      {
         return [left.description caseInsensitiveCompare:right.description];
      }];
   }
   
   return moduleList;
}

- (void)dealloc
{
}

- (id)copyWithZone:(NSZone *)zone
{
   return self;
}

- (bool)supportsFileAtPath:(NSString*)path
{
   return does_core_support_file(self.info, path.UTF8String);
}

@end

#ifdef IOS
#import "../iOS/views.h"

// Build a string with a second associated string
static NSString* build_string_pair(NSString* stringA, NSString* stringB)
{
   NSString* string_pair = [NSString stringWithString:stringA];
   objc_setAssociatedObject(string_pair, "OTHER", stringB, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
   return string_pair;
}

@implementation RAModuleInfoList
{
   RAModuleInfo* _data;
   uint32_t _firmwareSectionIndex;
}

- (id)initWithModuleInfo:(RAModuleInfo*)info
{
   self = [super initWithStyle:UITableViewStyleGrouped];

   _data = info;

   [self.sections addObject: [NSArray arrayWithObjects:@"Core",
      build_string_pair(@"Core Name", objc_get_value_from_config(_data.data, @"corename", @"Unspecified")),
      nil]];
   
   [self.sections addObject: [NSArray arrayWithObjects:@"Hardware/Software",
      build_string_pair(@"Developer", objc_get_value_from_config(_data.data, @"manufacturer", @"Unspecified")),
      build_string_pair(@"Name", objc_get_value_from_config(_data.data, @"systemname", @"Unspecified")),
      nil]];

   // Firmware
   _firmwareSectionIndex = 1000;
   uint32_t firmwareCount = 0;
   if (_data.data && config_get_uint(_data.data, "firmware_count", &firmwareCount) && firmwareCount)
   {
      NSMutableArray* firmwareSection = [NSMutableArray arrayWithObject:@"Firmware"];

      for (int i = 0; i < firmwareCount; i ++)
      {
         NSString* path = objc_get_value_from_config(_data.data, [NSString stringWithFormat:@"firmware%d_path", i + 1], @"Unspecified");
         path = [path stringByReplacingOccurrencesOfString:@"%sysdir%" withString:RetroArch_iOS.get.systemDirectory];      
         [firmwareSection addObject:build_string_pair(objc_get_value_from_config(_data.data, [NSString stringWithFormat:@"firmware%d_desc", i + 1], @"Unspecified"), path)];
      }

      _firmwareSectionIndex = self.sections.count;
      [self.sections addObject:firmwareSection];
   }

   return self;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   if (indexPath.section == _firmwareSectionIndex)
   {
      NSString* item = (NSString*)[self itemForIndexPath:indexPath];
      apple_display_alert(objc_getAssociatedObject(item, "OTHER"), item);
   }
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"datacell"];
   
   if (!cell)
   {
      cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:@"datacell"];
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      cell.detailTextLabel.adjustsFontSizeToFitWidth = YES;
   }
   
   NSString* item = (NSString*)[self itemForIndexPath:indexPath];
   NSString* value = (NSString*)objc_getAssociatedObject(item, "OTHER");
   
   cell.textLabel.text = item;
   cell.detailTextLabel.text = value;

   if (indexPath.section == _firmwareSectionIndex)
      cell.backgroundColor = path_file_exists(value.UTF8String) ? [UIColor blueColor] : [UIColor redColor];
   else
      cell.backgroundColor = [UIColor whiteColor];

   return cell;
}

@end

#endif