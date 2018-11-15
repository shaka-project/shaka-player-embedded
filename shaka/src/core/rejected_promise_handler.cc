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

#include "src/core/rejected_promise_handler.h"

#include "src/core/js_manager_impl.h"
#include "src/memory/heap_tracer.h"

namespace shaka {

namespace {
class RejectedRunner : public memory::Traceable {
 public:
  explicit RejectedRunner(RejectedPromiseHandler* handler)
      : handler_(handler) {}

  void Trace(memory::HeapTracer* tracer) const override {
    handler_->Trace(tracer);
  }

  void operator()() {
    handler_->LogPending();
  }

 private:
  RejectedPromiseHandler* handler_;
};
}  // namespace


RejectedPromiseHandler::RejectedPromiseHandler() : has_callback_(false) {}

RejectedPromiseHandler::~RejectedPromiseHandler() {}

void RejectedPromiseHandler::AddPromise(Handle<JsPromise> promise,
                                        Handle<JsValue> value) {
  promises_.emplace_back(promise, value);
  if (!has_callback_) {
    has_callback_ = true;
    JsManagerImpl::Instance()->MainThread()->AddInternalTask(
        TaskPriority::Immediate, "", RejectedRunner(this));
  }
}

void RejectedPromiseHandler::RemovePromise(Handle<JsPromise> promise) {
  for (auto it = promises_.begin(); it != promises_.end(); it++) {
    if (it->promise == promise) {
      promises_.erase(it);
      break;
    }
  }
}


RejectedPromiseHandler::PromiseInfo::PromiseInfo(Handle<JsPromise> promise,
                                                 Handle<JsValue> value)
    : promise(promise), value(value) {}

RejectedPromiseHandler::PromiseInfo::~PromiseInfo() {}

RejectedPromiseHandler::PromiseInfo::PromiseInfo(PromiseInfo&&) = default;

RejectedPromiseHandler::PromiseInfo& RejectedPromiseHandler::PromiseInfo::
operator=(PromiseInfo&&) = default;

void RejectedPromiseHandler::Trace(memory::HeapTracer* tracer) const {
  for (auto& info : promises_) {
    tracer->Trace(&info.promise);
    tracer->Trace(&info.value);
  }
}

void RejectedPromiseHandler::LogPending() {
  for (auto& info : promises_) {
    OnUncaughtException(info.value.handle(), /* inPromse */ true);
  }
  promises_.clear();
  has_callback_ = false;
}

}  // namespace shaka
