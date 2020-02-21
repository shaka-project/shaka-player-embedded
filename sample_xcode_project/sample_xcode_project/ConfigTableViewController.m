// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "ConfigTableViewController.h"
#import "ConfigTableValue.h"
#import "NSObject+ShakaLayoutHelpers.h"
#import "PlayerViewController.h"
#import <ShakaPlayerEmbedded/ShakaPlayerEmbedded.h>

#define ShakaTextInputBorderWidth 2

static NSMutableDictionary <NSString *, NSMutableArray <ConfigTableValue *> *> *gValues;
static NSMutableArray <NSString *> *gCategoriesOrder;

@interface ConfigTableViewController() <UITextFieldDelegate>

@end

@implementation ConfigTableViewController

#pragma mark - setup

+ (NSDictionary<NSString *, NSObject *> *)getConfigurations {
  NSMutableDictionary<NSString *, NSObject *> *configurations = [[NSMutableDictionary alloc] init];
  for (NSArray <ConfigTableValue *> *category in gValues.allValues) {
    for (ConfigTableValue *value in category) {
      switch (value.type) {
        case kConfigTableValueTypeBool:
        case kConfigTableValueTypeNumber:
          [configurations setValue:value.valueNumber forKey:value.path];
          break;
        case kConfigTableValueTypeString:
          [configurations setValue:value.valueString forKey:value.path];
          break;
      }
    }
  }
  return configurations;
}

+ (void)setupValues {
  gCategoriesOrder = [[NSMutableArray alloc] init];
  gValues = [[NSMutableDictionary alloc] init];

  [self addTableCategory:@"Languages"];
  [self addTableValueWithName:@"Preferred audio language"
                         path:@"preferredAudioLanguage"
                      andType:kConfigTableValueTypeString];
  [self addTableValueWithName:@"Preferred text language"
                         path:@"preferredTextLanguage"
                      andType:kConfigTableValueTypeString];

  [self addTableCategory:@"Adaptation"];
  [self addTableValueWithName:@"Adaptation Enabled"
                         path:@"abr.enabled"
                      andType:kConfigTableValueTypeBool];

  [self addTableCategory:@"Streaming"];
  [self addTableValueWithName:@"Jump large gaps"
                         path:@"streaming.jumpLargeGaps"
                      andType:kConfigTableValueTypeBool];

  // TODO: If we add any config values that are numbers, in the future, finish the implementation of
  // number config values.
}

+ (void)applyDefaults {
  NSString *systemLanguage = [[NSLocale preferredLanguages] firstObject];
  ShakaPlayer *player = [[ShakaPlayer alloc] initWithError:nil];

  for (NSArray <ConfigTableValue *> *category in gValues.allValues) {
    for (ConfigTableValue *value in category) {
      NSString *path = value.path;
      if ([path containsString:@"Language"]) {
        // Default to the system language.
        value.valueString = systemLanguage;
      } else {
        switch (value.type) {
          case kConfigTableValueTypeBool:
            value.valueNumber = [NSNumber numberWithBool:[player getConfigurationBool:path]];
            break;
          case kConfigTableValueTypeNumber:
            value.valueNumber = [NSNumber numberWithDouble:[player getConfigurationDouble:path]];
            break;
          case kConfigTableValueTypeString:
            value.valueString = [player getConfigurationString:path];
            break;
        }
      }
    }
  }
}

+ (void)applySavedValues {
  NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
  for (NSArray <ConfigTableValue *> *category in gValues.allValues) {
    for (ConfigTableValue *value in category) {
      switch (value.type) {
        case kConfigTableValueTypeString:
          value.valueString = [userDefaults objectForKey:value.path];
          break;
        case kConfigTableValueTypeBool:
        case kConfigTableValueTypeNumber:
          value.valueNumber = [userDefaults objectForKey:value.path];
          break;
      }
    }
  }
}

+ (void)saveValues {
  NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
  [userDefaults setBool:YES forKey:@"saved"];
  for (NSArray <ConfigTableValue *> *category in gValues.allValues) {
    for (ConfigTableValue *value in category) {
      switch (value.type) {
        case kConfigTableValueTypeString:
          [userDefaults setObject:value.valueString forKey:value.path];
          break;
        case kConfigTableValueTypeBool:
        case kConfigTableValueTypeNumber:
          [userDefaults setObject:value.valueNumber forKey:value.path];
          break;
      }
    }
  }
}

+ (void)setup {
  [self setupValues];

  if ([[NSUserDefaults standardUserDefaults] boolForKey:@"saved"]) {
    [self applySavedValues];
  } else {
    [self applyDefaults];
  }

  [self saveValues];
}

+ (void)addTableCategory:(NSString *)name {
  [gCategoriesOrder addObject:name];
  [gValues setValue:[[NSMutableArray alloc] init] forKey:name];
}

+ (void)addTableValueWithName:(NSString *)name
                         path:(NSString *)path
                      andType:(ConfigTableValueType)type {
  [self addTableValueWithName:name path:path andType:type withMin:0 andMax:0];
}

+ (void)addTableValueWithName:(NSString *)name
                         path:(NSString *)path
                      andType:(ConfigTableValueType)type
                      withMin:(float)min
                       andMax:(float)max {
  ConfigTableValue *value = [[ConfigTableValue alloc] init];
  value.name = name;
  value.path = path;
  value.type = type;
  NSMutableArray <ConfigTableValue *> *category = gValues[gCategoriesOrder.lastObject];
  value.tag = (100 * gCategoriesOrder.count) + category.count;
  value.min = min;
  value.max = max;
  [category addObject:value];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
  return gCategoriesOrder.count;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
  return gValues[gCategoriesOrder[section]].count;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
  return gCategoriesOrder[section];
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"cell"
                                                          forIndexPath:indexPath];

  NSString *category = gCategoriesOrder[indexPath.section];
  ConfigTableValue *value = gValues[category][indexPath.row];

  UILabel *label = [[UILabel alloc] init];
  label.text = value.name;
  label.accessibilityElementsHidden = YES;
  [cell addSubview:label];

  UIView *inputView;
  switch (value.type) {
    case kConfigTableValueTypeString:
      inputView = [self makeKeyboardForValue:value];
      break;
    case kConfigTableValueTypeBool:
      inputView = [self makeSwitchForValue:value];
      break;
    case kConfigTableValueTypeNumber:
      inputView = [self makeSliderForValue:value];
      break;
  }

  // Determine the required width of the label.
  [label sizeToFit];
  CGFloat labelWidth = label.bounds.size.width;

  [cell addSubview:inputView];
  NSDictionary <NSString *, UIView *> *views = @{
    @"i" : inputView,
    @"t" : label,
  };
  [self shaka_constraint:[NSString stringWithFormat:@"|-G-[t(==%f)]-G-[i(>=B)]-0-|", labelWidth]
                 onViews:views];
  [self shaka_constraint:@"V:|-0-[t(==B)]-0-|" onViews:views];
  [self shaka_constraint:@"V:[i(==B)]" onViews:views];
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterY fromItem:inputView toItem:cell];
  return cell;
}

#pragma mark - Table view delegate

- (BOOL)tableView:(UITableView *)tableView shouldHighlightRowAtIndexPath:(NSIndexPath *)indexPath {
  return NO;
}

#pragma mark - controls

- (UIView *)makeSliderForValue:(ConfigTableValue *)value {
  UIView *container = [[UIView alloc] init];

  UISlider *control = [[UISlider alloc] init];
  control.tag = value.tag;
  control.minimumValue = value.min;
  control.maximumValue = value.max;
  control.value = value.valueNumber.floatValue;
  control.continuous = NO;
  [control addTarget:self
              action:@selector(valueSlider:)
    forControlEvents:UIControlEventValueChanged];
  control.accessibilityLabel = value.name;
  [container addSubview:control];

  UILabel *min = [[UILabel alloc] init];
  min.text = [NSString stringWithFormat:@"%i", (int)value.min];
  [container addSubview:min];

  UILabel *max = [[UILabel alloc] init];
  max.text = [NSString stringWithFormat:@"%i", (int)value.max];
  [container addSubview:max];

  NSDictionary <NSString *, UIView *> *views = @{
    @"c" : control,
    @"l" : min,
    @"r" : max,
  };
  [self shaka_constraint:@"|-G-[l]-G-[c]-G-[r]-0-|" onViews:views];
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterY
                                 fromItem:control
                                   toItem:container];
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterY fromItem:min toItem:container];
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterY fromItem:max toItem:container];

  return container;
}

- (NSString *)localizedText:(NSString *)original {
  NSString *label = [[NSLocale currentLocale] displayNameForKey:NSLocaleIdentifier
                                                          value:original];
  return label ? label : original;
}

- (UIView *)makeKeyboardForValue:(ConfigTableValue *)value {
  UIView *container = [[UIView alloc] init];
  container.layer.borderColor = [UIColor blueColor].CGColor;
  container.layer.cornerRadius = 8;
  container.layer.borderWidth = ShakaTextInputBorderWidth;
  container.layer.masksToBounds = YES;

  UITextField *control = [[UITextField alloc] init];
  control.text = value.valueString;
  control.tag = value.tag;
  control.delegate = self;
  control.autocapitalizationType = UITextAutocapitalizationTypeNone;
  control.autocorrectionType = UITextAutocorrectionTypeNo;
  control.accessibilityLabel = value.name;
  control.accessibilityValue = [self localizedText:value.valueString];
  [container addSubview:control];

  // Add a thin blue border to the element, so it's obvious that it is an interactable element.
  control.translatesAutoresizingMaskIntoConstraints = NO;
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterX
                                 fromItem:control
                                   toItem:container];
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterY
                                 fromItem:control
                                   toItem:container];
  NSLayoutConstraint *widthC = [self shaka_equalConstraintForAttribute:NSLayoutAttributeWidth
                                                              fromItem:control
                                                                toItem:container];
  widthC.constant = -ShakaTextInputBorderWidth * 2;
  NSLayoutConstraint *heightC = [self shaka_equalConstraintForAttribute:NSLayoutAttributeHeight
                                                               fromItem:control
                                                                 toItem:container];
  heightC.constant = -ShakaTextInputBorderWidth * 2;

  return container;
}

- (UIView *)makeSwitchForValue:(ConfigTableValue *)value {
  UIView *container = [[UIView alloc] init];

  UISwitch *control = [[UISwitch alloc] init];
  control.on = value.valueNumber.boolValue;
  control.tag = value.tag;
  [control addTarget:self
              action:@selector(valueSwitch:)
    forControlEvents:UIControlEventValueChanged];
  control.accessibilityLabel = value.name;
  [container addSubview:control];

  // The switch attribute is fixed-width, so for the element to take up as much width as possible
  // and push the label to the left side, it has to be inside a variable-width container.
  NSDictionary <NSString *, UIView *> *views = @{ @"c" : control };
  [self shaka_constraint:@"[c]-0-|" onViews:views];
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterY
                                 fromItem:control
                                   toItem:container];

  return container;
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
  [textField resignFirstResponder];
  return YES;
}

- (void)textFieldDidBeginEditing:(UITextField *)textField {
  // Un-localize the text, just in case someone is editing this while using VoiceOver.
  textField.accessibilityValue = nil;
}

- (void)textFieldDidEndEditing:(UITextField *)textField {
  for (NSArray <ConfigTableValue *> *category in gValues.allValues) {
    for (ConfigTableValue *value in category) {
      if (value.tag == textField.tag) {
        value.valueString = textField.text;
        textField.accessibilityValue = [self localizedText:value.valueString];
        break;
      }
    }
  }
  [ConfigTableViewController saveValues];
}

- (void)valueSlider:(UISlider *)sender {
  for (NSArray <ConfigTableValue *> *category in gValues.allValues) {
    for (ConfigTableValue *value in category) {
      if (value.tag == sender.tag) {
        value.valueNumber = [NSNumber numberWithFloat:sender.value];
        break;
      }
    }
  }
  [ConfigTableViewController saveValues];
}

- (void)valueSwitch:(UISwitch *)sender {
  for (NSArray <ConfigTableValue *> *category in gValues.allValues) {
    for (ConfigTableValue *value in category) {
      if (value.tag == sender.tag) {
        value.valueNumber = [NSNumber numberWithBool:sender.on];
        break;
      }
    }
  }
  [ConfigTableViewController saveValues];
}

@end
