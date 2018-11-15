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

#import "ConfigTableValue.h"

@implementation ConfigTableValue

- (instancetype)init {
  if (self = [super init]) {
    self.name = @"";
    self.path = @"";
    self.type = kConfigTableValueTypeString;
    self.valueString = @"";
    self.valueNumber = @0;
    self.tag = 0;
    self.min = 0;
    self.max = 0;
  }
  return self;
}

@end
