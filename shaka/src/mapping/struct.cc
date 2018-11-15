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

#include "src/mapping/struct.h"

#include "src/mapping/weak_js_ptr.h"

namespace shaka {

Struct::Struct() {}
Struct::~Struct() {}

Struct::Struct(const Struct&) = default;
Struct::Struct(Struct&&) = default;
Struct& Struct::operator=(const Struct&) = default;
Struct& Struct::operator=(Struct&&) = default;

bool Struct::TryConvert(Handle<JsValue> value) {
  if (!IsObject(value))
    return false;
  LocalVar<JsObject> obj = UnsafeJsCast<JsObject>(value);
  for (auto& converter : converters_)
    converter->SearchAndStore(this, obj);
  return true;
}

ReturnVal<JsValue> Struct::ToJsValue() const {
  WeakJsPtr<JsObject> obj(CreateObject());
  for (auto& converter : converters_)
    converter->AddToObject(this, obj.handle());
  return obj.value();
}

void Struct::Trace(memory::HeapTracer* tracer) const {
  for (auto& converter : converters_)
    converter->Trace(this, tracer);
}

}  // namespace shaka
