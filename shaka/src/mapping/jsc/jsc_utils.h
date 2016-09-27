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

#ifndef SHAKA_EMBEDDED_MAPPING_JSC_JSC_UTILS_H_
#define SHAKA_EMBEDDED_MAPPING_JSC_JSC_UTILS_H_

#include <JavaScriptCore/JavaScriptCore.h>
#include <glog/logging.h>

#include <string>

namespace shaka {

JSContextRef GetContext();

void OnUncaughtException(JSValueRef exception, bool in_promise);

JSValueRef CreateNativeObject(const std::string& name, JSValueRef* args,
                              size_t argc);

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_JSC_JSC_UTILS_H_
