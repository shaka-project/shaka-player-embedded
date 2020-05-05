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

#ifndef SHAKA_EMBEDDED_CORE_REJECTED_PROMISE_HANDLER_H_
#define SHAKA_EMBEDDED_CORE_REJECTED_PROMISE_HANDLER_H_

#include <list>

#include "src/mapping/js_wrappers.h"
#include "src/mapping/weak_js_ptr.h"
#include "src/util/macros.h"

namespace shaka {

/**
 * A singleton class owned by the JsEngine that handles printing errors about
 * rejected Promises with no handlers.  When a Promise gets rejected without
 * any handlers, a log message should be printed.  However, we get a callback
 * from the JavaScript engine immediately, so we need to delay the message so
 * JavaScript can add handlers.
 */
class RejectedPromiseHandler {
 public:
  RejectedPromiseHandler();
  ~RejectedPromiseHandler();

  /**
   * Adds a new Promise to be logged.
   * @param promise The Promise object that was rejected.
   * @param value The value it was rejected with.
   */
  void AddPromise(Handle<JsPromise> promise, Handle<JsValue> value);
  /** Indicates that the given Promise had a handler added to it. */
  void RemovePromise(Handle<JsPromise> promise);

  /** Called by the background task to log unhandled rejections. */
  void LogUnhandledRejection();

 private:
  struct PromiseInfo {
    PromiseInfo(Handle<JsPromise> promise, Handle<JsValue> value);
    ~PromiseInfo();

    SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(PromiseInfo);

    Global<JsPromise> promise;
    Global<JsValue> value;
  };

  std::list<PromiseInfo> promises_;
  bool has_callback_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_CORE_REJECTED_PROMISE_HANDLER_H_
