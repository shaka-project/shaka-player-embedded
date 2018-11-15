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

#include "src/mapping/backing_object.h"

#include "src/mapping/backing_object_factory.h"
#include "src/memory/object_tracker.h"

namespace shaka {

BackingObject::BackingObject() {
  memory::ObjectTracker::Instance()->RegisterObject(this);
}

BackingObject::~BackingObject() {
#ifdef USING_JSC
  // If a short-lived object is destroyed, the private data in the JavaScript
  // object will still point to this object, but it will be invalid.
  if (!js_this_.empty())
    JSObjectSetPrivate(js_this_.handle(), nullptr);
#endif
}

void BackingObject::Trace(memory::HeapTracer* tracer) const {
  // Even though we can re-create the object later we need to trace it.
  // JSC will not reset the weak pointer once the object is freed,
  // giving us an invalid pointer.  So we need to trace it so the pointer
  // remains valid.
  tracer->Trace(&js_this_);
}

bool BackingObject::IsRootedAlive() const {
#ifdef USING_JSC
  return !js_this_.empty();
#else
  return false;
#endif
}

std::string BackingObject::name() const {
  return factory()->name();
}

bool BackingObject::DerivedFrom(const std::string& base) {
  return factory()->DerivedFrom(base);
}

ReturnVal<JsValue> BackingObject::JsThis() {
  // This allows a wrapper to be deleted or this object to be created in C++
  // where |js_this_| is initially empty.
  if (js_this_.empty()) {
    // NOTE: WrapInstance will call the JavaScript constructor which will call
    // SetJsThis, so no need to use the return value.
    factory()->WrapInstance(this);
    DCHECK(!js_this_.empty());
  }
  return js_this_.value();
}

void BackingObject::SetJsThis(Handle<JsObject> this_) {
  js_this_ = this_;
}

}  // namespace shaka
