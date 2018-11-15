// Copyright 2017 Google LLC
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

#include "src/mapping/register_member.h"

#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"

namespace shaka {
namespace impl {

bool ConstructWrapperObject(const CallbackArguments& arguments,
                            BackingObject* that) {
#if defined(USING_V8)
  that->SetJsThis(arguments.This());
  arguments.This()->SetAlignedPointerInInternalField(0, that);
  arguments.This()->SetAlignedPointerInInternalField(1, nullptr);
#elif defined(USING_JSC)
  JSClassRef cls = that->factory()->GetClass();
  LocalVar<JsObject> obj(JSObjectMake(GetContext(), cls, that));
  if (!obj)
    return false;
  that->SetJsThis(obj);
  arguments.SetReturn(obj);
#endif
  return true;
}

}  // namespace impl
}  // namespace shaka
