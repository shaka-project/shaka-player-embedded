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

// Derived from:
// https://chromium.googlesource.com/v8/v8/+/branch-heads/4.8/samples/hello-world.cc

#include "src/mapping/js_engine.h"

#include <libplatform/libplatform.h>

#include <cstring>

namespace shaka {

#ifdef V8_EMBEDDED_SNAPSHOT
// Defined in generated code by //shaka/tools/embed_v8_snapshot.py
void SetupV8Snapshots();
#endif

namespace {

void OnPromiseReject(v8::PromiseRejectMessage message) {
  JsEngine::Instance()->OnPromiseReject(message);
}

void InitializeV8IfNeeded() {
  static v8::Platform* platform = nullptr;
  if (platform)
    return;

  v8::V8::InitializeICU();

#ifdef V8_EMBEDDED_SNAPSHOT
  SetupV8Snapshots();
#endif

  platform = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(platform);
  v8::V8::Initialize();
}

}  // namespace

// \cond Doxygen_Skip

JsEngine::JsEngine() : isolate_(CreateIsolate()), context_(CreateContext()) {}

JsEngine::~JsEngine() {
  context_.Reset();
  isolate_->Dispose();
}

v8::Local<v8::Object> JsEngine::global_handle() {
  return context_.Get(isolate_)->Global();
}

v8::Local<v8::Value> JsEngine::global_value() {
  return context_.Get(isolate_)->Global();
}

void JsEngine::OnPromiseReject(v8::PromiseRejectMessage message) {
  // When a Promise gets rejected, we immediately get a
  // kPromiseRejectWithNoHandler event.  Then, once JavaScript adds a rejection
  // handler, we will get a kPromiseHandlerAddedAfterReject event.
  if (message.GetEvent() == v8::PromiseRejectEvent::kPromiseRejectWithNoHandler)
    promise_handler_.AddPromise(message.GetPromise(), message.GetValue());
  else
    promise_handler_.RemovePromise(message.GetPromise());
}

void JsEngine::AddDestructor(void* object,
                             std::function<void(void*)> destruct) {
  destructors_.emplace(object, destruct);
}

JsEngine::SetupContext::SetupContext()
    : locker(Instance()->isolate_),
      handles(Instance()->isolate_),
      isolate_scope(Instance()->isolate_),
      context_scope(Instance()->context_.Get(Instance()->isolate_)) {}

JsEngine::SetupContext::~SetupContext() {}

void* JsEngine::ArrayBufferAllocator::Allocate(size_t length) {
  void* data = AllocateUninitialized(length);
  return !data ? data : std::memset(data, 0, length);
}

void* JsEngine::ArrayBufferAllocator::AllocateUninitialized(size_t length) {
  return std::malloc(length);  // NOLINT
}

void JsEngine::ArrayBufferAllocator::Free(void* data, size_t /* length */) {
  auto* destructors = &Instance()->destructors_;
  if (destructors->count(data) > 0) {
    destructors->at(data)(data);
    destructors->erase(data);
  }
  std::free(data);  // NOLINT
}

v8::Isolate* JsEngine::CreateIsolate() {
  InitializeV8IfNeeded();

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = &allocator_;

  v8::Isolate* isolate = v8::Isolate::New(create_params);
  CHECK(isolate);
  isolate->SetCaptureStackTraceForUncaughtExceptions(true);
  isolate->SetPromiseRejectCallback(&::shaka::OnPromiseReject);

  return isolate;
}

v8::Global<v8::Context> JsEngine::CreateContext() {
  v8::Locker locker(isolate_);
  v8::HandleScope handles(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  return v8::Global<v8::Context>(isolate_, context);
}

// \endcond Doxygen_Skip

}  // namespace shaka
