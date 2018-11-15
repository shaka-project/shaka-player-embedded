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

#ifndef SHAKA_EMBEDDED_MAPPING_JS_ENGINE_H_
#define SHAKA_EMBEDDED_MAPPING_JS_ENGINE_H_

#include <glog/logging.h>

#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>

#include "src/core/rejected_promise_handler.h"
#include "src/mapping/js_wrappers.h"
#include "src/util/pseudo_singleton.h"

namespace shaka {

/**
 * Manages the global JavaScript engine.  This involves initializing any global
 * state and creating a new context that this manages.  This should be the first
 * member of JsManager::Impl to ensure the JavaScript engine is setup before
 * anything else.  This does not free the global state, only the context,
 * allowing multiple to exist, so long as there is only one at a time.
 */
class JsEngine : public PseudoSingleton<JsEngine> {
 public:
  JsEngine();
  ~JsEngine();

  Handle<JsObject> global_handle();
  ReturnVal<JsValue> global_value();

#if defined(USING_V8)
  void OnPromiseReject(v8::PromiseRejectMessage message);
  void AddDestructor(void* object, std::function<void(void*)> destruct);
  v8::Isolate* isolate() const {
    // Verify this thread can use the isolate.
    DCHECK(isolate_);
    DCHECK(v8::Locker::IsLocked(isolate_));
    return isolate_;
  }
#elif defined(USING_JSC)
  JSContextRef context() const;
#endif  // USING_JSC

  struct SetupContext {
    SetupContext();
    ~SetupContext();

   private:
#if defined(USING_V8)
    v8::Locker locker;
    v8::HandleScope handles;
    v8::Isolate::Scope isolate_scope;
    v8::Context::Scope context_scope;
#endif
  };

 private:
#if defined(USING_V8)
  class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
   public:
    void* Allocate(size_t length) override;
    void* AllocateUninitialized(size_t length) override;
    void Free(void* data, size_t) override;
  };

  v8::Isolate* CreateIsolate();
  v8::Global<v8::Context> CreateContext();

  ArrayBufferAllocator allocator_;
  std::unordered_map<void*, std::function<void(void*)>> destructors_;
  v8::Isolate* isolate_;
  v8::Global<v8::Context> context_;
#elif defined(USING_JSC)
  JSGlobalContextRef context_;
  std::thread::id thread_id_;
#endif

  RejectedPromiseHandler promise_handler_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_JS_ENGINE_H_
