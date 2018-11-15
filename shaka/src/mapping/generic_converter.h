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

#ifndef SHAKA_EMBEDDED_MAPPING_GENERIC_CONVERTER_H_
#define SHAKA_EMBEDDED_MAPPING_GENERIC_CONVERTER_H_

#include "src/mapping/js_wrappers.h"
#include "src/memory/heap_tracer.h"

namespace shaka {

/**
 * Defines a base class for types that do their own parsing.  Objects like
 * these allow generic parsing by the conversion framework.  When trying to
 * convert to the given type, a stack object of the type will be created and
 * the TryConvert method will be called.  If it returns true, then the object
 * will be moved into the argument.
 *
 * Note these are NOT backing objects and will be created on the stack.  This
 * should only be used for simple objects as they are created and destroyed
 * often.
 */
class GenericConverter {
 public:
  virtual ~GenericConverter() {}

  /**
   * Tries to convert the given value into the required type, populating
   * the members of this object as needed.
   * @return True on success, false on error.
   */
  virtual bool TryConvert(Handle<JsValue> value) = 0;
  /** Converts the current value into a JavaScript value. */
  virtual ReturnVal<JsValue> ToJsValue() const = 0;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_GENERIC_CONVERTER_H_
