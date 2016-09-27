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

#include "src/mapping/jsc/jsc_utils.h"

#include <vector>

#include "src/mapping/js_engine.h"
#include "src/mapping/js_wrappers.h"

namespace shaka {

JSContextRef GetContext() {
  return JsEngine::Instance()->context();
}

void OnUncaughtException(JSValueRef exception, bool in_promise) {
  if (!exception)
    return;

  if (in_promise)
    LOG(ERROR) << "Uncaught (in promise): " << ConvertToString(exception);
  else
    LOG(ERROR) << "Uncaught: " << ConvertToString(exception);

  // TODO: Print stack trace?
}

JSValueRef CreateNativeObject(const std::string& name, JSValueRef* args,
                              size_t argc) {
  JSContextRef cx = GetContext();
  LocalVar<JsObject> global = JSContextGetGlobalObject(cx);
  JSValueRef ctor = GetMemberRaw(global, name);
  DCHECK(ctor && IsObject(ctor));
  LocalVar<JsObject> ctor_obj = UnsafeJsCast<JsObject>(ctor);

  LocalVar<JsValue> ret;
  std::vector<LocalVar<JsValue>> local_args{args, args + argc};
  CHECK(InvokeConstructor(ctor_obj, argc, local_args.data(), &ret));
  return ret;
}


}  // namespace shaka
