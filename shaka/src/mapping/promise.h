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

#ifndef SHAKA_EMBEDDED_MAPPING_PROMISE_H_
#define SHAKA_EMBEDDED_MAPPING_PROMISE_H_

#include <string>

#include "src/js/js_error.h"
#include "src/mapping/any.h"
#include "src/mapping/generic_converter.h"
#include "src/mapping/js_wrappers.h"
#include "src/mapping/weak_js_ptr.h"

namespace shaka {

#if defined(USING_V8)
using JsPromiseResolver = v8::Promise::Resolver;
#endif


/**
 * Defines a wrapper around a JavaScript Promise.  This manages two kinds of
 * Promises: (1) those created by us and (2) those created in JavaScript.
 */
class Promise : public GenericConverter, public memory::Traceable {
 public:
  static std::string name() {
    return "Promise";
  }

  /** A *pending* Promise that can be resolved/rejected by us. */
  Promise();
  ~Promise() override;

  // Chromium style complains without these.
  Promise(const Promise&);
  Promise(Promise&&);
  Promise& operator=(const Promise&);
  Promise& operator=(Promise&&);

  static Promise Resolved() {
    Promise ret;
    LocalVar<JsValue> undef = JsUndefined();
    ret.ResolveWith(undef, /* run_events */ false);
    return ret;
  }

  static Promise Resolved(Handle<JsValue> value) {
    Promise ret;
    ret.ResolveWith(value, /* run_events */ false);
    return ret;
  }

  static Promise Rejected(const js::JsError& error) {
    Promise ret;
    ret.RejectWith(error, /* run_events */ false);
    return ret;
  }

  bool TryConvert(Handle<JsValue> value) override;
  ReturnVal<JsValue> ToJsValue() const override;
  void Trace(memory::HeapTracer* tracer) const override;

  /** Whether the Promise can be resolved/rejected. */
  bool CanResolve() const {
#ifdef USING_JSC
    return !resolve_.empty();
#else
    return !resolver_.empty();
#endif
  }

  /**
   * Resolves the Promise with the given value.  This can only be called if the
   * contained Promise was created by us.  Promises loaded from JavaScript (even
   * if they were originally created by us) can't call this.
   */
  void ResolveWith(Handle<JsValue> value, bool run_events = true);
  void RejectWith(const js::JsError& error, bool run_events = true);

  /**
   * Adds callbacks that are invoked once this Promise is resolved/rejected.
   * These are called on the event thread.  Like in JavaScript, if the Promise
   * is already resolved/rejected, these will be called on the next loop.
   */
  void Then(std::function<void(Any)> on_resolve,
            std::function<void(Any)> on_reject);

 private:
#ifdef USING_JSC
  WeakJsPtr<JsObject> resolve_;
  WeakJsPtr<JsObject> reject_;
#else
  WeakJsPtr<JsPromiseResolver> resolver_;
#endif
  WeakJsPtr<JsPromise> promise_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_PROMISE_H_
