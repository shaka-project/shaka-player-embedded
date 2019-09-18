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

#include "src/mapping/v8/v8_utils.h"

#include <glog/logging.h>

#include "src/core/js_manager_impl.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/convert_js.h"
#include "src/mapping/js_engine.h"
#include "src/mapping/js_wrappers.h"

namespace shaka {

v8::Isolate* GetIsolate() {
  return JsEngine::Instance()->isolate();
}

void PrintStackTrace(const v8::Local<v8::StackTrace>& stack) {
  v8::HandleScope handle_scope(GetIsolate());
  for (int i = 0; i < stack->GetFrameCount(); ++i) {
    const v8::Local<v8::StackFrame> frame = stack->GetFrame(i);
    const v8::String::Utf8Value name(frame->GetScriptName());
    const v8::String::Utf8Value func(frame->GetFunctionName());
    const int row = frame->GetLineNumber();
    const int col = frame->GetColumn();

    std::string func_or_main = func.length() ? *func : "<anonymous>";
    LOG(ERROR) << "  at " << func_or_main << " (" << *name << ":" << row << ":"
               << col << ")";
  }
}

void OnUncaughtException(const v8::Local<v8::Value>& exception,
                         bool in_promise) {
  if (exception.IsEmpty())
    return;

  // Print the exception and stack trace.
  v8::String::Utf8Value err_str(exception);
  if (in_promise)
    LOG(ERROR) << "Uncaught (in promise): " << *err_str;
  else
    LOG(ERROR) << "Uncaught:" << *err_str;

  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::StackTrace> stack = v8::Exception::GetStackTrace(exception);
  if (!stack.IsEmpty()) {
    PrintStackTrace(stack);
  }
}

}  // namespace shaka
