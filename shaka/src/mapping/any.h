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

#ifndef SHAKA_EMBEDDED_MAPPING_ANY_H_
#define SHAKA_EMBEDDED_MAPPING_ANY_H_

#include <string>

#include "src/mapping/convert_js.h"
#include "src/mapping/generic_converter.h"
#include "src/mapping/js_wrappers.h"
#include "src/mapping/weak_js_ptr.h"

namespace shaka {

/**
 * Defines any JavaScript value.  This allows accepting any value and includes
 * methods to convert to different types.
 */
class Any : public GenericConverter, public memory::Traceable {
 public:
  static std::string name() {
    return "anything";
  }

  Any();
  explicit Any(std::nullptr_t);
  template <typename T>
  explicit Any(const T& value) {
    value_ = ::shaka::ToJsValue(value);
    is_number_ = GetValueType(value_.handle()) == JSValueType::Number;
  }
  ~Any() override;

  // Chromium style complains without these.
  Any(const Any&);
  Any(Any&&);
  Any& operator=(const Any&);
  Any& operator=(Any&&);

  /**
   * Returns whether the value contained is a "truthy" value.  The following
   * values are falsy, everything else is truthy:
   * - undefined
   * - null
   * - "" (empty string)
   * - false (boolean)
   * - NaN (number)
   * - 0 (number)
   */
  bool IsTruthy() const;

  /**
   * Tries to convert the current value into the given type.  This can only
   * be called on the event thread.
   *
   * @param result [OUT] Where to put the converted value.
   * @return True on success, false on failure.
   */
  template <typename T>
  bool TryConvertTo(T* result) const {
    return FromJsValue(value_.handle(), result);
  }

  bool TryConvert(Handle<JsValue> value) override;
  ReturnVal<JsValue> ToJsValue() const override;
  void Trace(memory::HeapTracer* tracer) const override;

 private:
  WeakJsPtr<JsValue> value_;
  bool is_number_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_ANY_H_
