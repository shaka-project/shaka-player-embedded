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

#include "src/mapping/promise.h"

#include <utility>

#include "src/mapping/callback.h"
#include "src/mapping/register_member.h"
#include "src/memory/heap_tracer.h"

namespace shaka {

namespace {
#if defined(USING_V8)
v8::Local<v8::Promise::Resolver> NewPromiseResolver() {
  return v8::Promise::Resolver::New(GetIsolate()->GetCurrentContext())
      .ToLocalChecked();
}

v8::Local<v8::Promise> GetPromise(v8::Local<v8::Promise::Resolver> resolver) {
  return resolver->GetPromise();
}
#elif defined(USING_JSC)
Handle<JSObjectRef> NewPromise(WeakJsPtr<JsObject>* resolve,
                               WeakJsPtr<JsObject>* reject) {
  std::function<void(Callback, Callback)> callback = [&](Callback on_resolve,
                                                         Callback on_reject) {
    *resolve = UnsafeJsCast<JsObject>(on_resolve.ToJsValue());
    *reject = UnsafeJsCast<JsObject>(on_reject.ToJsValue());
  };

  JSValueRef ctor =
      GetMemberRaw(JSContextGetGlobalObject(GetContext()), "Promise");
  DCHECK_EQ(GetValueType(ctor), JSValueType::Function);
  LocalVar<JsFunction> ctor_obj = UnsafeJsCast<JsFunction>(ctor);

  LocalVar<JsValue> ret;
  LocalVar<JsValue> args[] = {CreateStaticFunction("", "", callback)};
  CHECK(InvokeConstructor(ctor_obj, 1, args, &ret));
  return UnsafeJsCast<JsObject>(ret);
}
#endif
}  // namespace

#ifdef USING_JSC
Promise::Promise() : promise_(NewPromise(&resolve_, &reject_)) {}
#else
Promise::Promise()
    : resolver_(NewPromiseResolver()),
      promise_(GetPromise(resolver_.handle())) {}
#endif

Promise::~Promise() {}

Promise::Promise(const Promise&) = default;
Promise::Promise(Promise&&) = default;
Promise& Promise::operator=(const Promise&) = default;
Promise& Promise::operator=(Promise&&) = default;

bool Promise::TryConvert(Handle<JsValue> value) {
  if (GetValueType(value) != JSValueType::Promise)
    return false;

#ifdef USING_JSC
  resolve_ = nullptr;
  reject_ = nullptr;
#else
  resolver_ = nullptr;
#endif
  promise_ = UnsafeJsCast<JsPromise>(value);
  return true;
}

ReturnVal<JsValue> Promise::ToJsValue() const {
  return promise_.value();
}

void Promise::Trace(memory::HeapTracer* tracer) const {
  tracer->Trace(&promise_);
#ifdef USING_JSC
  tracer->Trace(&resolve_);
  tracer->Trace(&reject_);
#else
  tracer->Trace(&resolver_);
#endif
}

void Promise::ResolveWith(Handle<JsValue> value, bool run_events) {
  CHECK(CanResolve()) << "Can't reject JavaScript created Promises.";
#if defined(USING_V8)
  (void)resolver_.handle()->Resolve(GetIsolate()->GetCurrentContext(), value);
  // In V8, Promises are invoked automatically, but *after* executing some
  // JavaScript.  If we resolve it now, the handlers won't be invoked until we
  // call into JavaScript again, and then only after that JavaScript runs.  For
  // example, if the next JavaScript is a timer, the timer will run first, then
  // the Promise handlers will be invoked, which is not the correct order.
  if (run_events)
    GetIsolate()->RunMicrotasks();
#elif defined(USING_JSC)
  LocalVar<JsValue> except;
  if (!InvokeMethod(resolve_.handle(), Handle<JsObject>(), 1, &value,
                    &except)) {
    OnUncaughtException(except, /* in_promise */ false);
  }
#endif
}

void Promise::RejectWith(const js::JsError& error, bool run_events) {
  CHECK(CanResolve()) << "Can't reject JavaScript created Promises.";
#if defined(USING_V8)
  (void)resolver_.handle()->Reject(GetIsolate()->GetCurrentContext(),
                                   error.error());
  // See comment in ResolveWith().
  if (run_events)
    GetIsolate()->RunMicrotasks();
#elif defined(USING_JSC)
  LocalVar<JsValue> except;
  LocalVar<JsValue> rooted(error.error());
  if (!InvokeMethod(reject_.handle(), Handle<JsObject>(), 1, &rooted,
                    &except)) {
    OnUncaughtException(except, /* in_promise */ false);
  }
#endif
}

void Promise::Then(std::function<void(Any)> on_resolve,
                   std::function<void(Any)> on_reject) {
  // Note this will get from the prototype chain too.
  LocalVar<JsValue> member_val = GetMemberRaw(promise_.handle(), "then");
  LocalVar<JsFunction> member = UnsafeJsCast<JsFunction>(member_val);

  LocalVar<JsValue> except;
  LocalVar<JsFunction> on_resolve_js =
      CreateStaticFunction("", "", std::move(on_resolve));
  LocalVar<JsFunction> on_reject_js =
      CreateStaticFunction("", "", std::move(on_reject));
  LocalVar<JsValue> arguments[] = {RawToJsValue(on_resolve_js),
                                   RawToJsValue(on_reject_js)};
  CHECK(InvokeMethod(member, promise_.handle(), 2, arguments, &except))
      << ConvertToString(except);
}

}  // namespace shaka
