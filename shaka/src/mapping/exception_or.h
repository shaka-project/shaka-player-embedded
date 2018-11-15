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

#ifndef SHAKA_EMBEDDED_MAPPING_EXCEPTION_OR_H_
#define SHAKA_EMBEDDED_MAPPING_EXCEPTION_OR_H_

#include "shaka/variant.h"
#include "src/js/js_error.h"

namespace shaka {

/**
 * Contains either an exception or a return value.
 */
template <typename T>
struct ExceptionOr final : variant<js::JsError, T> {
  using variant<js::JsError, T>::variant;
};

template <>
struct ExceptionOr<void> final : variant<js::JsError, monostate> {
  using variant<js::JsError, monostate>::variant;
  ExceptionOr() : variant(monostate()) {}
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_EXCEPTION_OR_H_
