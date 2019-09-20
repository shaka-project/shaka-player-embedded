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

#ifndef SHAKA_EMBEDDED_JS_JS_ERROR_H_
#define SHAKA_EMBEDDED_JS_JS_ERROR_H_

#include <string>

#include "src/js/dom/exception_code.h"
#include "src/mapping/js_wrappers.h"

namespace shaka {
namespace js {

/**
 * Contains several static methods that will be used to throw a C++ exception
 * that will be converted to the correct JavaScript error.
 */
class JsError {
 public:
  static std::string GetJsStack();

  static JsError RangeError(const std::string& message);
  static JsError ReferenceError(const std::string& message);
  static JsError TypeError(const std::string& message);
  static JsError SyntaxError(const std::string& message);
  static JsError Error(const std::string& message);

  static JsError Rethrow(Handle<JsValue> error);

#ifdef USING_V8
  static JsError Rethrow(const v8::TryCatch& trycatch);
#endif

  static JsError DOMException(ExceptionCode code);
  static JsError DOMException(ExceptionCode code, const std::string& message);

  ~JsError();
  JsError(JsError&&);

  ReturnVal<JsValue> error() const {
    return error_;
  }

 private:
  explicit JsError(ReturnVal<JsValue> error);

  ReturnVal<JsValue> error_;
};

}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_JS_ERROR_H_
