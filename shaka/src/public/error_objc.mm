// Copyright 2019 Google LLC
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

#include "shaka/error_objc.h"

#include "shaka/error.h"
#include "src/util/objc_utils.h"

const NSErrorDomain ShakaPlayerErrorDomain = @"ShakaPlayerErrorDomain";
const NSErrorUserInfoKey ShakaPlayerErrorCategoryKey = @"ShakaPlayerErrorCategoryKey";
const NSErrorUserInfoKey ShakaPlayerErrorSeverityKey = @"ShakaPlayerErrorSeverityKey";

@interface ShakaPlayerError () {
}

@end

@implementation ShakaPlayerError

@synthesize message;
@synthesize category;
@synthesize code;
@synthesize severity;

- (instancetype)initWithError:(const shaka::Error &)error {
  NSString *message = shaka::util::ObjcConverter<std::string>::ToObjc(error.message);
  if ((self = [super initWithDomain:ShakaPlayerErrorDomain
                               code:error.code
                           userInfo:@{ShakaPlayerErrorCategoryKey: @(error.category),
                                      ShakaPlayerErrorSeverityKey: @(error.severity),
                                      NSLocalizedDescriptionKey: message}])) {
    self.message = message;
    self.category = error.category;
    self.severity = error.severity;
  }
  return self;
}

@end
