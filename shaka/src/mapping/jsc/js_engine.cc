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

#include "src/mapping/js_engine.h"

#include "src/core/js_manager_impl.h"
#include "src/memory/heap_tracer.h"
#include "src/memory/object_tracker.h"

namespace shaka {

namespace {
constexpr const uint64_t kGcIntervalMs = 30 * 1000;
}  // namespace

// \cond Doxygen_Skip

JsEngine::JsEngine()
    : context_(JSGlobalContextCreate(nullptr)),
      thread_id_(std::this_thread::get_id()) {
  auto task = []() {
    VLOG(1) << "Begin GC run";
    auto* object_tracker = memory::ObjectTracker::Instance();
    auto* heap_tracer = JsManagerImpl::Instance()->HeapTracer();
    heap_tracer->BeginPass();
    heap_tracer->TraceAll(object_tracker->GetAliveObjects());
    object_tracker->FreeDeadObjects(heap_tracer->alive());

    // This will signal to JSC that we have just destroyed a lot of objects.
    // See http://bugs.webkit.org/show_bug.cgi?id=84476
    JSGarbageCollect(GetContext());

    VLOG(1) << "End GC run";
  };

  // If the engine was created as part of a test, then don't create the timer
  // since we don't need to GC.
  JsManagerImpl* impl = JsManagerImpl::InstanceOrNull();
  if (impl)
    impl->MainThread()->AddRepeatedTimer(kGcIntervalMs, std::move(task));
}

JsEngine::~JsEngine() {
  JSGlobalContextRelease(context_);
}

Handle<JsObject> JsEngine::global_handle() {
  return JSContextGetGlobalObject(context());
}

ReturnVal<JsValue> JsEngine::global_value() {
  return JSContextGetGlobalObject(context());
}

JSContextRef JsEngine::context() const {
  // TODO: Consider asserting we are on the correct thread.  Unlike other
  // JavaScript engines, JSC allows access from any thread and will just
  // serialize the requests.  We can't assert as of now since the C++ API's
  // Player will unref in the destructor.
  return context_;
}

JsEngine::SetupContext::SetupContext() {}
JsEngine::SetupContext::~SetupContext() {}

// \endcond Doxygen_Skip

}  // namespace shaka
