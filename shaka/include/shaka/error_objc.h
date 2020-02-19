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

#ifndef SHAKA_EMBEDDED_ERROR_OBJC_H_
#define SHAKA_EMBEDDED_ERROR_OBJC_H_

#import <Foundation/Foundation.h>

#include "macros.h"

/**
 * Represents a Player error.  This can be either a Shaka error or a more
 * generic JavaScript error.
 *
 * @see https://github.com/google/shaka-player/blob/master/lib/util/error.js
 * @ingroup player
 */
SHAKA_EXPORT
FOUNDATION_EXPORT NSErrorDomain const ShakaPlayerErrorDomain;
FOUNDATION_EXPORT NSErrorUserInfoKey const ShakaPlayerErrorCategoryKey;
FOUNDATION_EXPORT NSErrorUserInfoKey const ShakaPlayerErrorSeverityKey;

@interface ShakaPlayerError : NSError

/** The error message. */
@property(atomic) NSString *message;

/**
 * The category of the error, if this is a Shaka error.  This is the same as
 * shaka.util.Error.Category.
 */
@property(atomic) NSInteger category;

/**
 * The specific code of the error, if this is a Shaka error.  This is the same
 * as shaka.util.Error.Code.
 */
@property(atomic) NSInteger code;

/**
 * The Shaka severity of the error, if this is a Shaka error.  This is the
 * same as shaka.util.Error.Severity.
 */
@property (atomic) NSInteger severity;

@end

#endif  // SHAKA_EMBEDDED_ERROR_OBJC_H_
