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

#import "../RetroArch/RetroArch_Apple.h"
#import "views.h"

#include "../RetroArch/apple_input.h"
#include "../RetroArch/keycode.h"
#include "input/BTStack/btdynamic.h"
#include "input/BTStack/btpad.h"

enum SettingTypes
{
   BooleanSetting, ButtonSetting, EnumerationSetting, FileListSetting,
   GroupSetting, AspectSetting, RangeSetting, CustomAction
};

@interface RASettingData : NSObject
@property enum SettingTypes type;

@property (strong) NSString* label;
@property (strong) NSString* name;
@property (strong) NSString* value;

@property (strong) NSString* path;
@property (strong) NSArray* subValues;
@property (strong) NSMutableArray* msubValues;

@property double rangeMin;
@property double rangeMax;

@property uint32_t player;

@property bool haveNoneOption;

- (id)initWithType:(enum SettingTypes)aType label:(NSString*)aLabel name:(NSString*)aName;
@end

@implementation RASettingData
- (id)initWithType:(enum SettingTypes)aType label:(NSString*)aLabel name:(NSString*)aName
{
   self.type = aType;
   self.label = aLabel;
   self.name = aName;
   return self;
}
@end

// Helper view definitions
@interface RAButtonGetter : NSObject<UIAlertViewDelegate>
- (id)initWithSetting:(RASettingData*)setting fromTable:(UITableView*)table;
@end

@interface RASettingEnumerationList : UITableViewController
- (id)initWithSetting:(RASettingData*)setting fromTable:(UITableView*)table;
@end


static RASettingData* boolean_setting(config_file_t* config, NSString* name, NSString* label, NSString* defaultValue)
{
   RASettingData* result = [[RASettingData alloc] initWithType:BooleanSetting label:label name:name];
   result.value = objc_get_value_from_config(config, name, defaultValue);
   return result;
}

static RASettingData* button_setting(config_file_t* config, uint32_t player, NSString* name, NSString* label, NSString* defaultValue)
{
   NSString* realname = player ? [NSString stringWithFormat:@"input_player%d_%@", player, name] : name;
   
   RASettingData* result = [[RASettingData alloc] initWithType:ButtonSetting label:label name:realname];
   result.msubValues = [NSMutableArray arrayWithObjects:
                        objc_get_value_from_config(config, realname, defaultValue),
                        objc_get_value_from_config(config, [realname stringByAppendingString:@"_btn"], @"nul"),
                        objc_get_value_from_config(config, [realname stringByAppendingString:@"_axis"], @"nul"),
                        nil];
   result.player = player ? player - 1 : 0;
   return result;
}

static RASettingData* group_setting(NSString* label, NSArray* settings)
{
   RASettingData* result = [[RASettingData alloc] initWithType:GroupSetting label:label name:nil];
   result.subValues = settings;
   return result;
}

static RASettingData* enumeration_setting(config_file_t* config, NSString* name, NSString* label, NSString* defaultValue, NSArray* values)
{
   RASettingData* result = [[RASettingData alloc] initWithType:EnumerationSetting label:label name:name];
   result.value = objc_get_value_from_config(config, name, defaultValue);
   result.subValues = values;
   return result;
}

static RASettingData* subpath_setting(config_file_t* config, NSString* name, NSString* label, NSString* defaultValue, NSString* path, NSString* extension)
{
   NSString* value = objc_get_value_from_config(config, name, defaultValue);
   value = [value stringByReplacingOccurrencesOfString:path withString:@""];

   NSArray* values = [[NSFileManager defaultManager] subpathsOfDirectoryAtPath:path error:nil];
   values = [values pathsMatchingExtensions:[NSArray arrayWithObject:extension]];

   RASettingData* result = [[RASettingData alloc] initWithType:FileListSetting label:label name:name];
   result.value = value;
   result.subValues = values;
   result.path = path;
   result.haveNoneOption = true;
   return result;
}

static RASettingData* range_setting(config_file_t* config, NSString* name, NSString* label, NSString* defaultValue, double minValue, double maxValue)
{
   RASettingData* result = [[RASettingData alloc] initWithType:RangeSetting label:label name:name];
   result.value = objc_get_value_from_config(config, name, defaultValue);
   result.rangeMin = minValue;
   result.rangeMax = maxValue;
   return result;
}

static RASettingData* aspect_setting(config_file_t* config, NSString* label)
{
   // Why does this need to be so difficult?

   RASettingData* result = [[RASettingData alloc] initWithType:AspectSetting label:label name:@"fram"];
   result.subValues = [NSArray arrayWithObjects:@"Fill Screen", @"Game Aspect", @"Pixel Aspect", @"4:3", @"16:9", nil];

   bool videoForceAspect = true;
   bool videoAspectAuto = false;
   double videoAspect = -1.0;

   if (config)
   {
      config_get_bool(config, "video_force_aspect", &videoForceAspect);
      config_get_bool(config, "video_aspect_auto", &videoAspectAuto);
      config_get_double(config, "video_aspect_ratio", &videoAspect);
   }
   
   if (!videoForceAspect)
      result.value = @"Fill Screen";
   else if (videoAspect < 0.0)
      result.value = videoAspectAuto ? @"Game Aspect" : @"Pixel Aspect";
   else
      result.value = (videoAspect < 1.5) ? @"4:3" : @"16:9";
   
   return result;
}

static RASettingData* custom_action(NSString* action, NSString* value, id data)
{
   RASettingData* result = [[RASettingData alloc] initWithType:CustomAction label:action name:nil];
   result.value = value;
   
   if (data != nil)
      objc_setAssociatedObject(result, "USERDATA", data, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
   
   return result;
}

static NSArray* build_input_port_group(config_file_t* config, uint32_t player)
{
   return [NSArray arrayWithObjects:
            [NSArray arrayWithObjects:[NSString stringWithFormat:@"Player %d", player],
               button_setting(config, player, @"up", @"Up", @"up"),
               button_setting(config, player, @"down", @"Down", @"down"),
               button_setting(config, player, @"left", @"Left", @"left"),
               button_setting(config, player, @"right", @"Right", @"right"),

               button_setting(config, player, @"start", @"Start", @"enter"),
               button_setting(config, player, @"select", @"Select", @"rshift"),

               button_setting(config, player, @"b", @"B", @"z"),
               button_setting(config, player, @"a", @"A", @"x"),
               button_setting(config, player, @"x", @"X", @"s"),
               button_setting(config, player, @"y", @"Y", @"a"),

               button_setting(config, player, @"l", @"L", @"q"),
               button_setting(config, player, @"r", @"R", @"w"),
               button_setting(config, player, @"l2", @"L2", @"nul"),
               button_setting(config, player, @"r2", @"R2", @"nul"),
               button_setting(config, player, @"l3", @"L3", @"nul"),
               button_setting(config, player, @"r3", @"R3", @"nul"),

               button_setting(config, player, @"l_y_minus", @"Left Stick Up", @"nul"),
               button_setting(config, player, @"l_y_plus", @"Left Stick Down", @"nul"),
               button_setting(config, player, @"l_x_minus", @"Left Stick Left", @"nul"),
               button_setting(config, player, @"l_x_plus", @"Left Stick Right", @"nul"),
               button_setting(config, player, @"r_y_minus", @"Right Stick Up", @"nul"),
               button_setting(config, player, @"r_y_plus", @"Right Stick Down", @"nul"),
               button_setting(config, player, @"r_x_minus", @"Right Stick Left", @"nul"),
               button_setting(config, player, @"r_x_plus", @"Right Stick Right", @"nul"),
               nil],
            nil];
}

@implementation RASettingsList
{
   RAModuleInfo* _module;
   NSString* _configPath;
   bool _cancelSave; // Set to prevent dealloc from writing to disk
}

+ (void)refreshModuleConfig:(RAModuleInfo*)module;
{
   (void)[[RASettingsList alloc] initWithModule:module];
}

- (id)initWithModule:(RAModuleInfo*)module
{
   _module = module;
   _configPath = RetroArch_iOS.get.retroarchConfigPath;

   config_file_t* config = config_file_new([_configPath UTF8String]);

   NSString* overlay_path = [[[NSBundle mainBundle] bundlePath] stringByAppendingString:@"/overlays/"];
   NSString* shader_path = [[[NSBundle mainBundle] bundlePath] stringByAppendingString:@"/shaders_glsl/"];

   NSArray* settings = [NSArray arrayWithObjects:
      [NSArray arrayWithObjects:@"Core",
         custom_action(@"Core Info", nil, nil),
         nil],

      [NSArray arrayWithObjects:@"Video",
         boolean_setting(config, @"video_smooth", @"Bilinear filtering", @"true"),
         boolean_setting(config, @"video_crop_overscan", @"Crop Overscan", @"true"),
         boolean_setting(config, @"video_scale_integer", @"Integer Scaling", @"false"),
         aspect_setting(config, @"Aspect Ratio"),
         nil],
         
      [NSArray arrayWithObjects:@"GPU Shader",         
         boolean_setting(config, @"video_shader_enable", @"Enable Shader", @"false"),
         subpath_setting(config, @"video_shader", @"Shader", @"", shader_path, @"glsl"),
         nil],

      [NSArray arrayWithObjects:@"Audio",
         boolean_setting(config, @"audio_enable", @"Enable Output", @"true"),
         boolean_setting(config, @"audio_sync", @"Sync on Audio", @"true"),
         boolean_setting(config, @"audio_rate_control", @"Rate Control", @"true"),
         nil],

      [NSArray arrayWithObjects:@"Input",
         subpath_setting(config, @"input_overlay", @"Input Overlay", @"", overlay_path, @"cfg"),
         range_setting(config, @"input_overlay_opacity", @"Overlay Opacity", @"1.0", 0.0, 1.0),
         group_setting(@"System Keys", [NSArray arrayWithObjects:
            // TODO: Many of these strings will be cut off on an iPhone
            [NSArray arrayWithObjects:@"System Keys",
               button_setting(config, 0, @"input_menu_toggle", @"Show RGUI", @"f1"),
               button_setting(config, 0, @"input_disk_eject_toggle", @"Insert/Eject Disk", @"nul"),
               button_setting(config, 0, @"input_disk_next", @"Cycle Disks", @"nul"),
               button_setting(config, 0, @"input_save_state", @"Save State", @"f2"),
               button_setting(config, 0, @"input_load_state", @"Load State", @"f4"),
               button_setting(config, 0, @"input_state_slot_increase", @"Next State Slot", @"f7"),
               button_setting(config, 0, @"input_state_slot_decrease", @"Previous State Slot", @"f6"),
               button_setting(config, 0, @"input_toggle_fast_forward", @"Toggle Fast Forward", @"space"),
               button_setting(config, 0, @"input_hold_fast_forward", @"Hold Fast Forward", @"l"),
               button_setting(config, 0, @"input_rewind", @"Rewind", @"r"),
               button_setting(config, 0, @"input_slowmotion", @"Slow Motion", @"e"),
               button_setting(config, 0, @"input_reset", @"Reset", @"h"),
               button_setting(config, 0, @"input_exit_emulator", @"Close Game", @"escape"),
               button_setting(config, 0, @"input_enable_hotkey", @"Hotkey Enable (Always on if not set)", @"nul"),
               nil],
            nil]),
         group_setting(@"Player 1 Keys", build_input_port_group(config, 1)),
         group_setting(@"Player 2 Keys", build_input_port_group(config, 2)),
         group_setting(@"Player 3 Keys", build_input_port_group(config, 3)),
         group_setting(@"Player 4 Keys", build_input_port_group(config, 4)),
         nil],
      
      [NSArray arrayWithObjects:@"Save States",
         boolean_setting(config, @"rewind_enable", @"Enable Rewinding", @"false"),
         boolean_setting(config, @"block_sram_overwrite", @"Disable SRAM on Load", @"false"),
         boolean_setting(config, @"savestate_auto_save", @"Auto Save on Exit", @"false"),
         boolean_setting(config, @"savestate_auto_load", @"Auto Load on Startup", @"true"),
         nil],
      nil
   ];

   if (config)
      config_file_free(config);

   self = [super initWithSettings:settings title:_module ? _module.description : @"Global Core Config"];
   return self;
}

- (void)dealloc
{
   if (!_cancelSave)
   {
      config_file_t* config = config_file_new([_configPath UTF8String]);
    
      if (!config)
         config = config_file_new(0);

      if (config)
      {
         config_set_string(config, "system_directory", [[RetroArch_iOS get].systemDirectory UTF8String]);
         [self writeSettings:nil toConfig:config];
         config_file_write(config, [_configPath UTF8String]);
         config_file_free(config);
      }

      [[RetroArch_iOS get] refreshConfig];
   }
}

- (void)handleCustomAction:(RASettingData*)setting
{
   if ([@"Core Info" isEqualToString:setting.label])
      [[RetroArch_iOS get] pushViewController:[[RAModuleInfoList alloc] initWithModuleInfo:_module] animated:YES];
}

@end

@implementation RASystemSettingsList
- (id)init
{
   config_file_t* config = config_file_new([[RetroArch_iOS get].systemConfigPath UTF8String]);

   NSArray* settings = [NSArray arrayWithObjects:
      [NSArray arrayWithObjects:@"Frontend",
         custom_action(@"Diagnostic Log", nil, nil),
         boolean_setting(config, @"ios_tv_mode", @"TV Mode", @"false"),
         nil],
      [NSArray arrayWithObjects:@"Bluetooth",
         // TODO: Note that with this turned off the native bluetooth is expected to be a real keyboard
         boolean_setting(config, @"ios_use_icade", @"Native BT is iCade", @"false"),
         btstack_try_load() ? boolean_setting(config, @"ios_use_btstack", @"Enable BTstack", @"false") : nil,
         nil],
      [NSArray arrayWithObjects:@"Orientations",
         boolean_setting(config, @"ios_allow_portrait", @"Portrait", @"true"),
         boolean_setting(config, @"ios_allow_portrait_upside_down", @"Portrait Upside Down", @"true"),
         boolean_setting(config, @"ios_allow_landscape_left", @"Landscape Left", @"true"),
         boolean_setting(config, @"ios_allow_landscape_right", @"Landscape Right", @"true"),
         nil],
      [NSArray arrayWithObjects:@"Cores",
         custom_action(@"Core Configuration", nil, nil),
         nil],
      nil
   ];

   if (config)
      config_file_free(config);

   self = [super initWithSettings:settings title:@"RetroArch Settings"];
   return self;
}

- (void)dealloc
{
   config_file_t* config = config_file_new([[RetroArch_iOS get].systemConfigPath UTF8String]);
   
    if (!config)
        config = config_file_new(0);
   
   if (config)
   {
      [self writeSettings:nil toConfig:config];
      config_file_write(config, [[RetroArch_iOS get].systemConfigPath UTF8String]);
      config_file_free(config);
   }
   
   [[RetroArch_iOS get] refreshSystemConfig];
}

- (void)handleCustomAction:(RASettingData*)setting
{
   if ([@"Diagnostic Log" isEqualToString:setting.label])
      [[RetroArch_iOS get] pushViewController:[RALogView new] animated:YES];
   else if ([@"Enable BTstack" isEqualToString:setting.label])
      btstack_set_poweron([setting.value isEqualToString:@"true"]);
   else if([@"Core Configuration" isEqualToString:setting.label])
      [RetroArch_iOS.get pushViewController:[[RASettingsList alloc] initWithModule:nil] animated:YES];
}

@end

@implementation RASettingsSubList
- (id)initWithSettings:(NSMutableArray*)values title:(NSString*)title
{
   self = [super initWithStyle:UITableViewStyleGrouped];
   [self setTitle:title];
   
   self.sections = values;
   return self;
}

- (bool)isSettingsView
{
   return true;
}

- (void)handleCustomAction:(RASettingData*)setting
{

}

- (void)writeSettings:(NSArray*)settingList toConfig:(config_file_t*)config
{
   if (!config)
      return;

   NSArray* list = settingList ? settingList : self.sections;

   for (int i = 0; i < [list count]; i++)
   {
      NSArray* group = [list objectAtIndex:i];
   
      for (int j = 1; j < [group count]; j ++)
      {
         RASettingData* setting = [group objectAtIndex:j];
         
         switch (setting.type)
         {         
            case GroupSetting:
               [self writeSettings:setting.subValues toConfig:config];
               break;
               
            case FileListSetting:
               if ([setting.value length] > 0)
                  config_set_string(config, [setting.name UTF8String], [[setting.path stringByAppendingPathComponent:setting.value] UTF8String]);
               else
                  config_set_string(config, [setting.name UTF8String], "");
               break;

            case ButtonSetting:
               if (setting.msubValues[0])
                  config_set_string(config, [setting.name UTF8String], [setting.msubValues[0] UTF8String]);
               if (setting.msubValues[1])
                  config_set_string(config, [[setting.name stringByAppendingString:@"_btn"] UTF8String], [setting.msubValues[1] UTF8String]);
               if (setting.msubValues[2])
                  config_set_string(config, [[setting.name stringByAppendingString:@"_axis"] UTF8String], [setting.msubValues[2] UTF8String]);
               break;

            case AspectSetting:
               config_set_string(config, "video_force_aspect", [@"Fill Screen" isEqualToString:setting.value] ? "false" : "true");
               config_set_string(config, "video_aspect_ratio_auto", [@"Game Aspect" isEqualToString:setting.value] ? "true" : "false");
               config_set_string(config, "video_aspect_ratio", "-1.0");
               if([@"4:3" isEqualToString:setting.value])
                  config_set_string(config, "video_aspect_ratio", "1.33333333");
               else if([@"16:9" isEqualToString:setting.value])
                  config_set_string(config, "video_aspect_ratio", "1.77777777");
               break;

            case CustomAction:
               break;

            default:
               config_set_string(config, [setting.name UTF8String], [setting.value UTF8String]);
               break;
         }
      }
   }
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   RASettingData* setting = (RASettingData*)[self itemForIndexPath:indexPath];
   
   switch (setting.type)
   {
      case EnumerationSetting:
      case FileListSetting:
      case AspectSetting:
         [[RetroArch_iOS get] pushViewController:[[RASettingEnumerationList alloc] initWithSetting:setting fromTable:(UITableView*)self.view] animated:YES];
         break;
         
      case ButtonSetting:
         (void)[[RAButtonGetter alloc] initWithSetting:setting fromTable:(UITableView*)self.view];
         break;
         
      case GroupSetting:
         [[RetroArch_iOS get] pushViewController:[[RASettingsSubList alloc] initWithSettings:setting.subValues title:setting.label] animated:YES];
         break;
         
      default:
         break;
   }
   
   [self handleCustomAction:setting];
}

- (void)handleBooleanSwitch:(UISwitch*)swt
{
   RASettingData* setting = objc_getAssociatedObject(swt, "SETTING");
   setting.value = (swt.on ? @"true" : @"false");
   
   [self handleCustomAction:setting];
}

- (void)handleSlider:(UISlider*)sld
{
   RASettingData* setting = objc_getAssociatedObject(sld, "SETTING");
   setting.value = [NSString stringWithFormat:@"%f", sld.value];

   [self handleCustomAction:setting];
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   RASettingData* setting = (RASettingData*)[self itemForIndexPath:indexPath];
  
   UITableViewCell* cell = nil;

   switch (setting.type)
   {
      case BooleanSetting:
      {
         cell = [self.tableView dequeueReusableCellWithIdentifier:@"boolean"];

         if (cell == nil)
         {
            cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"boolean"];
         
            UISwitch* accessory = [[UISwitch alloc] init];
            [accessory addTarget:self action:@selector(handleBooleanSwitch:) forControlEvents:UIControlEventValueChanged];
            cell.accessoryView = accessory;
            
            [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
         }
      
         cell.textLabel.text = setting.label;
      
         UISwitch* swt = (UISwitch*)cell.accessoryView;
         swt.on = [setting.value isEqualToString:@"true"];
         objc_setAssociatedObject(swt, "SETTING", setting, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
         
         return cell;
      }

      case RangeSetting:
      {
         cell = [self.tableView dequeueReusableCellWithIdentifier:@"range"];
         
         if (cell == nil)
         {
            cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"range"];

            UISlider* accessory = [UISlider new];
            [accessory addTarget:self action:@selector(handleSlider:) forControlEvents:UIControlEventValueChanged];
            accessory.continuous = NO;
            cell.accessoryView = accessory;

            [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
         }
         
         cell.textLabel.text = setting.label;
         
         UISlider* sld = (UISlider*)cell.accessoryView;
         sld.minimumValue = setting.rangeMin;
         sld.maximumValue = setting.rangeMax;
         sld.value = [setting.value doubleValue];
         objc_setAssociatedObject(sld, "SETTING", setting, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
         
         return cell;
      }
      break;
         
      default:
      {
         cell = [self.tableView dequeueReusableCellWithIdentifier:@"default"];
         
         if (!cell)
         {
            cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:@"default"];
            cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
         }
         
         cell.textLabel.text = setting.label;
   
         if (setting.type != ButtonSetting)
            cell.detailTextLabel.text = setting.value;
         else
            cell.detailTextLabel.text = [NSString stringWithFormat:@"[KB:%@] [JS:%@] [AX:%@]",
               [setting.msubValues[0] length] ? setting.msubValues[0] : @"nul",
               [setting.msubValues[1] length] ? setting.msubValues[1] : @"nul",
               [setting.msubValues[2] length] ? setting.msubValues[2] : @"nul"];

         return cell;
      }
   }
}

@end

@implementation RASettingEnumerationList
{
   RASettingData* _value;
   UITableView* _view;
   unsigned _mainSection;
};

- (id)initWithSetting:(RASettingData*)setting fromTable:(UITableView*)table
{
   self = [super initWithStyle:UITableViewStyleGrouped];
   
   _value = setting;
   _view = table;
   _mainSection = _value.haveNoneOption ? 1 : 0;
   
   [self setTitle: _value.label];
   return self;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
   return _value.haveNoneOption ? 2 : 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
   return (section == _mainSection) ? _value.subValues.count : 1;
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"option"];
   cell = cell ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"option"];

   cell.textLabel.text = (indexPath.section == _mainSection) ? _value.subValues[indexPath.row] : @"None";

   return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   _value.value = (indexPath.section == _mainSection) ? _value.subValues[indexPath.row] : @"";
   
   [_view reloadData];
   [[RetroArch_iOS get] popViewControllerAnimated:YES];
}

@end

@implementation RAButtonGetter
{
   RAButtonGetter* _me;
   RASettingData* _value;
   UIAlertView* _alert;
   UITableView* _view;
   bool _finished;
   NSTimer* _btTimer;
}

- (id)initWithSetting:(RASettingData*)setting fromTable:(UITableView*)table
{
   self = [super init];

   _value = setting;
   _view = table;
   _me = self;

   _alert = [[UIAlertView alloc] initWithTitle:@"RetroArch"
                                 message:_value.label
                                 delegate:self
                                 cancelButtonTitle:@"Cancel"
                                 otherButtonTitles:@"Clear Keyboard", @"Clear Joystick", @"Clear Axis", nil];
   [_alert show];
   
   _btTimer = [NSTimer scheduledTimerWithTimeInterval:.05f target:self selector:@selector(checkInput) userInfo:nil repeats:YES];
   return self;
}

- (void)finish
{
   if (!_finished)
   {
      _finished = true;
   
      [_btTimer invalidate];

      [_alert dismissWithClickedButtonIndex:_alert.cancelButtonIndex animated:YES];
      [_view reloadData];
   
      _me = nil;
   }
}

- (void)alertView:(UIAlertView*)alertView willDismissWithButtonIndex:(NSInteger)buttonIndex
{
   if (buttonIndex == _alert.firstOtherButtonIndex)
      _value.msubValues[0] = @"nul";
   else if(buttonIndex == _alert.firstOtherButtonIndex + 1)
      _value.msubValues[1] = @"nul";
   else if(buttonIndex == _alert.firstOtherButtonIndex + 2)
      _value.msubValues[2] = @"nul";

   [self finish];
}

- (void)checkInput
{
   // Keyboard
   static const struct
   {
      const char* const keyname;
      const uint32_t hid_id;
   } ios_key_name_map[] = {
      { "left", KEY_Left },               { "right", KEY_Right },
      { "up", KEY_Up },                   { "down", KEY_Down },
      { "enter", KEY_Enter },             { "kp_enter", KP_Enter },
      { "space", KEY_Space },             { "tab", KEY_Tab },
      { "shift", KEY_LeftShift },         { "rshift", KEY_RightShift },
      { "ctrl", KEY_LeftControl },        { "alt", KEY_LeftAlt },
      { "escape", KEY_Escape },           { "backspace", KEY_DeleteForward },
      { "backquote", KEY_Grave },         { "pause", KEY_Pause },

      { "f1", KEY_F1 },                   { "f2", KEY_F2 },
      { "f3", KEY_F3 },                   { "f4", KEY_F4 },
      { "f5", KEY_F5 },                   { "f6", KEY_F6 },
      { "f7", KEY_F7 },                   { "f8", KEY_F8 },
      { "f9", KEY_F9 },                   { "f10", KEY_F10 },
      { "f11", KEY_F11 },                 { "f12", KEY_F12 },

      { "num0", KEY_0 },                  { "num1", KEY_1 },
      { "num2", KEY_2 },                  { "num3", KEY_3 },
      { "num4", KEY_4 },                  { "num5", KEY_5 },
      { "num6", KEY_6 },                  { "num7", KEY_7 },
      { "num8", KEY_8 },                  { "num9", KEY_9 },
   
      { "insert", KEY_Insert },           { "del", KEY_DeleteForward },
      { "home", KEY_Home },               { "end", KEY_End },
      { "pageup", KEY_PageUp },           { "pagedown", KEY_PageDown },
   
      { "add", KP_Add },                  { "subtract", KP_Subtract },
      { "multiply", KP_Multiply },        { "divide", KP_Divide },
      { "keypad0", KP_0 },                { "keypad1", KP_1 },
      { "keypad2", KP_2 },                { "keypad3", KP_3 },
      { "keypad4", KP_4 },                { "keypad5", KP_5 },
      { "keypad6", KP_6 },                { "keypad7", KP_7 },
      { "keypad8", KP_8 },                { "keypad9", KP_9 },
   
      { "period", KEY_Period },           { "capslock", KEY_CapsLock },
      { "numlock", KP_NumLock },          { "print_screen", KEY_PrintScreen },
      { "scroll_lock", KEY_ScrollLock },
   
      { "a", KEY_A }, { "b", KEY_B }, { "c", KEY_C }, { "d", KEY_D },
      { "e", KEY_E }, { "f", KEY_F }, { "g", KEY_G }, { "h", KEY_H },
      { "i", KEY_I }, { "j", KEY_J }, { "k", KEY_K }, { "l", KEY_L },
      { "m", KEY_M }, { "n", KEY_N }, { "o", KEY_O }, { "p", KEY_P },
      { "q", KEY_Q }, { "r", KEY_R }, { "s", KEY_S }, { "t", KEY_T },
      { "u", KEY_U }, { "v", KEY_V }, { "w", KEY_W }, { "x", KEY_X },
      { "y", KEY_Y }, { "z", KEY_Z },

      { "nul", 0x00},
   };
   
   
   for (int i = 0; ios_key_name_map[i].hid_id; i++)
   {
      if (g_current_input_data.keys[ios_key_name_map[i].hid_id])
      {
         _value.msubValues[0] = [NSString stringWithUTF8String:ios_key_name_map[i].keyname];
         [self finish];
         return;
      }
   }

   // Pad Buttons
   for (int i = 0; g_current_input_data.pad_buttons[_value.player] && i < sizeof(g_current_input_data.pad_buttons[_value.player]) * 8; i++)
   {
      if (g_current_input_data.pad_buttons[_value.player] & (1 << i))
      {
         _value.msubValues[1] = [NSString stringWithFormat:@"%d", i];
         [self finish];
         return;
      }
   }

   // Pad Axis
   for (int i = 0; i < 4; i++)
   {
      int16_t value = g_current_input_data.pad_axis[_value.player][i];
      
      if (abs(value) > 0x1000)
      {
         _value.msubValues[2] = [NSString stringWithFormat:@"%s%d", (value > 0x1000) ? "+" : "-", i];
         [self finish];
         break;
      }
   }
}

@end

