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

#include "src/mapping/callback.h"

#include "src/memory/heap_tracer.h"

namespace shaka {

Callback::Callback() {}
Callback::~Callback() {}

Callback::Callback(const Callback&) = default;
Callback::Callback(Callback&&) = default;
Callback& Callback::operator=(const Callback&) = default;
Callback& Callback::operator=(Callback&&) = default;

bool Callback::TryConvert(Handle<JsValue> given) {
  if (GetValueType(given) != proto::ValueType::Function)
    return false;
  callback_ = UnsafeJsCast<JsFunction>(given);
  return true;
}

ReturnVal<JsValue> Callback::ToJsValue() const {
  return callback_.value();
}

void Callback::Trace(memory::HeapTracer* tracer) const {
  tracer->Trace(&callback_);
}

}  // namespace shaka
