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

@implementation ShakaPlayerError

@synthesize message;
@synthesize category;
@synthesize severity;

- (instancetype)initWithMessage:(NSString *)message {
  return [self initWithMessage:message severity:0 category:0 code:0];
}

- (instancetype)initWithMessage:(NSString *)message
                       severity:(NSInteger)severity
                       category:(NSInteger)category
                           code:(NSInteger)code {
  if ((self = [super initWithDomain:ShakaPlayerErrorDomain
                               code:code
                           userInfo:@{
                             ShakaPlayerErrorCategoryKey: @(category),
                             ShakaPlayerErrorSeverityKey: @(severity),
                             NSLocalizedDescriptionKey: message
                           }])) {
    self.message = message;
    self.category = category;
    self.severity = severity;
  }
  return self;
}

- (instancetype)initWithError:(const shaka::Error &)error {
  NSString *message = shaka::util::ObjcConverter<std::string>::ToObjc(error.message);
  // To avoid crash while creating userInfo if C string conversion fails
  message = message ? message : @"";
  return [self initWithMessage:message
                      severity:error.severity
                      category:error.category
                          code:error.code];
}

@end
