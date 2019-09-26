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

#ifndef SHAKA_EMBEDDED_ERROR_OBJC_INTERNAL_H_
#define SHAKA_EMBEDDED_ERROR_OBJC_INTERNAL_H_

#import <Foundation/Foundation.h>

#include "shaka/error.h"
#include "shaka/error_objc.h"

@interface ShakaPlayerError(Internal)

- (instancetype)initWithError:(const shaka::Error&)error;

@end

#endif  // SHAKA_EMBEDDED_ERROR_OBJC_INTERNAL_H_
