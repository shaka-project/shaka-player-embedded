// Copyright 2016 Google LLC
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

#ifndef SHAKA_EMBEDDED_JS_IDB_IDB_UTILS_H_
#define SHAKA_EMBEDDED_JS_IDB_IDB_UTILS_H_

#include <string>

#include "shaka/variant.h"
#include "src/js/idb/database.pb.h"
#include "src/mapping/any.h"
#include "src/mapping/exception_or.h"

namespace shaka {
namespace js {
namespace idb {

using IdbKeyType = int64_t;

/**
 * Converts the given JavaScript object into a stored item.  This is an
 * expensive operation that makes copies of the data.  Therefore, this should
 * only be done right before being stored.
 *
 * This implements part of the structured clone algorithm to copy the data.
 * This does not support Blob, FileList, ImageData, Map, or Set since we don't
 * define any of those types, and neither the RegExp or Date types.  This also
 * does not support retaining object references or cycles.
 *
 * This will throw a JsError on error.
 *
 * @see https://www.w3.org/TR/html5/infrastructure.html#structured-clone
 *
 * @param input The value to convert.
 * @param result [OUT] If this returns true, will contain the new converted item
 * @return The thrown exception, or nothing on success.
 */
ExceptionOr<void> StoreInProto(Any input, proto::Value* result);

/**
 * Converts the given stored Item and converts it into a new JavaScript object.
 * @param value The stored object to convert.
 * @return A new JavaScript value that is the equivalent to |value|.
 */
Any LoadFromProto(const proto::Value& value);

}  // namespace idb
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_IDB_IDB_UTILS_H_
