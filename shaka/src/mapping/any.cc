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

#include "src/mapping/any.h"

#include <cmath>

#include "src/memory/heap_tracer.h"

namespace shaka {

Any::Any() : value_(JsUndefined()), is_number_(false) {}
Any::Any(std::nullptr_t) : value_(JsNull()), is_number_(false) {}
Any::~Any() {}

Any::Any(const Any&) = default;
Any::Any(Any&&) = default;
Any& Any::operator=(const Any&) = default;
Any& Any::operator=(Any&&) = default;

bool Any::IsTruthy() const {
  switch (GetValueType(value_.handle())) {
    case JSValueType::Undefined:
    case JSValueType::Null:
      return false;
    case JSValueType::String:
      return !ConvertToString(value_.handle()).empty();
    case JSValueType::Boolean:
      return BooleanFromValue(value_.handle());
    case JSValueType::Number: {
      const double val = NumberFromValue(value_.handle());
      return !std::isnan(val) && val != 0;
    }
    default:
      return true;
  }
}

bool Any::TryConvert(Handle<JsValue> value) {
  value_ = value;
  is_number_ = GetValueType(value_.handle()) == JSValueType::Number;
  return true;
}

ReturnVal<JsValue> Any::ToJsValue() const {
  return value_.value();
}

void Any::Trace(memory::HeapTracer* tracer) const {
  // V8 doesn't seem to support tracing numbers.  Other primitives are okay to
  // trace, so only ignore if this is a number.
  if (!is_number_)
    tracer->Trace(&value_);
}

}  // namespace shaka
