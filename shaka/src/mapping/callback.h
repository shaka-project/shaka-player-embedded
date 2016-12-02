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

#ifndef SHAKA_EMBEDDED_MAPPING_CALLBACK_H_
#define SHAKA_EMBEDDED_MAPPING_CALLBACK_H_

#include <string>
#include <utility>

#include "src/mapping/convert_js.h"
#include "src/mapping/exception_or.h"
#include "src/mapping/js_engine.h"
#include "src/mapping/js_wrappers.h"
#include "src/mapping/weak_js_ptr.h"
#include "src/util/macros.h"

namespace shaka {

/**
 * Defines a helper type that is used to store and call JavaScript functions.
 *
 * This defines a call operator that will call the function and swallow
 * exceptions, printing the stack trace in the debug log if one is thrown.  This
 * also defines a named function to call the JavaScript function and return any
 * exception instead.
 */
class Callback : public GenericConverter, public memory::Traceable {
 public:
  static std::string name() {
    return "function";
  }

  Callback();
  ~Callback() override;

  Callback(const Callback&);
  Callback(Callback&&);
  Callback& operator=(const Callback&);
  Callback& operator=(Callback&&);

  bool empty() const {
    return callback_.empty();
  }

  bool operator==(const Callback& other) const {
    return callback_ == other.callback_;
  }
  bool operator!=(const Callback& other) const {
    return callback_ != other.callback_;
  }

  template <typename... Args>
  void operator()(Args&&... args) const {
    const auto exception = CallInternal(JsEngine::Instance()->global_handle(),
                                        std::forward<Args>(args)...);
    if (holds_alternative<js::JsError>(exception)) {
      OnUncaughtException(get<js::JsError>(exception).error(),
                          /* in_promise */ false);
    }
  }

  template <typename T, typename... Args>
  MUST_USE_RESULT ExceptionOr<void> CallWithThis(T&& that,
                                                 Args&&... args) const {
    LocalVar<JsValue> that_val = ::shaka::ToJsValue(that);
    return CallInternal(that_val, std::forward<Args>(args)...);
  }

  bool TryConvert(Handle<JsValue> given) override;
  ReturnVal<JsValue> ToJsValue() const override;
  void Trace(memory::HeapTracer* tracer) const override;

 private:
  template <typename... Args>
  ExceptionOr<void> CallInternal(Handle<JsValue> that, Args&&... args) const {
    DCHECK(!empty());
    DCHECK(IsObject(that));

    LocalVar<JsObject> that_obj = UnsafeJsCast<JsObject>(that);
    // Add another element to avoid a 0-length array with no arguments.  This
    // won't change the number of arguments passed in JavaScript.
    LocalVar<JsValue> arguments[] = {
        ::shaka::ToJsValue(std::forward<Args>(args))..., JsUndefined()};
    LocalVar<JsValue> except;
    if (!InvokeMethod(callback_.handle(), that_obj, sizeof...(Args), arguments,
                      &except)) {
      return js::JsError::Rethrow(except);
    }
    return {};
  }

  WeakJsPtr<JsFunction> callback_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_CALLBACK_H_
