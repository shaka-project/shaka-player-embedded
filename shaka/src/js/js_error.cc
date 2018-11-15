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

#include "src/js/js_error.h"

#include "src/core/ref_ptr.h"
#include "src/js/dom/dom_exception.h"
#include "src/mapping/convert_js.h"

namespace shaka {
namespace js {

namespace {

#if defined(USING_JSC)
ReturnVal<JsValue> CreateError(const std::string& message,
                               const std::string& type) {
  JSContextRef cx = GetContext();
  JSValueRef ctor = GetMemberRaw(JSContextGetGlobalObject(cx), type);
  Handle<JsString> str(JsStringFromUtf8(message));
  JSValueRef args[] = {JSValueMakeString(GetContext(), str)};
  return JSObjectCallAsConstructor(cx, UnsafeJsCast<JsObject>(ctor), 1, args,
                                   nullptr);
}
#endif

}  // namespace

std::string JsError::GetJsStack() {
#if defined(USING_V8)
  v8::Local<v8::String> empty = v8::String::Empty(GetIsolate());
  v8::Local<v8::Value> except = v8::Exception::Error(empty);
  CHECK(!except.IsEmpty());
  DCHECK(except->IsObject());
  return ConvertToString(GetMemberRaw(except.As<v8::Object>(), "stack"));
#elif defined(USING_JSC)
  // TODO: Get the call stack.
  return "";
#endif
}

JsError JsError::RangeError(const std::string& message) {
#if defined(USING_V8)
  return JsError(v8::Exception::RangeError(JsStringFromUtf8(message)));
#elif defined(USING_JSC)
  return JsError(CreateError(message, "RangeError"));
#endif
}

JsError JsError::ReferenceError(const std::string& message) {
#if defined(USING_V8)
  return JsError(v8::Exception::ReferenceError(JsStringFromUtf8(message)));
#elif defined(USING_JSC)
  return JsError(CreateError(message, "ReferenceError"));
#endif
}

JsError JsError::TypeError(const std::string& message) {
#if defined(USING_V8)
  return JsError(v8::Exception::TypeError(JsStringFromUtf8(message)));
#elif defined(USING_JSC)
  return JsError(CreateError(message, "TypeError"));
#endif
}

JsError JsError::SyntaxError(const std::string& message) {
#if defined(USING_V8)
  return JsError(v8::Exception::SyntaxError(JsStringFromUtf8(message)));
#elif defined(USING_JSC)
  return JsError(CreateError(message, "SyntaxError"));
#endif
}

JsError JsError::Error(const std::string& message) {
#if defined(USING_V8)
  return JsError(v8::Exception::Error(JsStringFromUtf8(message)));
#elif defined(USING_JSC)
  return JsError(CreateError(message, "Error"));
#endif
}

#ifdef USING_V8
JsError JsError::Rethrow(const v8::TryCatch& trycatch) {
  return JsError(trycatch.Exception());
}
#endif

JsError JsError::DOMException(ExceptionCode code) {
  // Careful here.  We are creating a new object but we won't hold a reference
  // to it.  We are running on the event thread, so a GC run cannot happen yet.
  // We will throw the wrapper which will keep the object alive.
  RefPtr<dom::DOMException> except = new dom::DOMException(code);
  except->stack = GetJsStack();
  return JsError(except->JsThis());
}

JsError JsError::DOMException(ExceptionCode code, const std::string& message) {
  RefPtr<dom::DOMException> except = new dom::DOMException(code, message);
  except->stack = GetJsStack();
  return JsError(except->JsThis());
}

JsError::JsError(ReturnVal<JsValue> error) : error_(error) {}

JsError::JsError(JsError&&) = default;

JsError::~JsError() {}

}  // namespace js
}  // namespace shaka
