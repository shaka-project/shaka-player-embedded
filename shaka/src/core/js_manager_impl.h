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

#ifndef SHAKA_EMBEDDED_CORE_JS_MANAGER_IMPL_H_
#define SHAKA_EMBEDDED_CORE_JS_MANAGER_IMPL_H_

#include <glog/logging.h>

#include <functional>
#include <memory>
#include <string>

#include "shaka/js_manager.h"
#include "src/core/environment.h"
#include "src/core/network_thread.h"
#include "src/core/task_runner.h"
#include "src/debug/thread_event.h"
#include "src/memory/heap_tracer.h"
#include "src/memory/object_tracker.h"
#ifdef USING_V8
#  include "src/memory/v8_heap_tracer.h"
#endif
#include "src/util/pseudo_singleton.h"

namespace shaka {

class JsManagerImpl : public PseudoSingleton<JsManagerImpl> {
 public:
  explicit JsManagerImpl(const JsManager::StartupOptions& options);
  ~JsManagerImpl();

  TaskRunner* MainThread() {
    return &event_loop_;
  }
  NetworkThread* NetworkThread() {
    return &network_thread_;
  }
  memory::HeapTracer* HeapTracer() {
    return &heap_tracer_;
  }

  std::string GetPathForStaticFile(const std::string& file) const;
  std::string GetPathForDynamicFile(const std::string& file) const;

  void Stop() {
    event_loop_.Stop();
  }

  void WaitUntilFinished();

  std::shared_ptr<ThreadEvent<bool>> RunScript(const std::string& path);
  std::shared_ptr<ThreadEvent<bool>> RunScript(const std::string& path,
                                               const uint8_t* data,
                                               size_t data_size);

 private:
  void EventThreadWrapper(TaskRunner::RunLoop run_loop);

#ifdef USING_V8
  memory::V8HeapTracer heap_tracer_;
#else
  memory::HeapTracer heap_tracer_;
#endif
  memory::ObjectTracker tracker_;
  JsManager::StartupOptions startup_options_;

  TaskRunner event_loop_;
  class NetworkThread network_thread_;
};

/**
 * Creates a callback that, when invoked, will invoke the given callback on the
 * main thread.
 */
template <typename... Args>
std::function<void(Args...)> MainThreadCallback(
    std::function<void(Args...)> cb) {
  return [=](Args... args) {
    JsManagerImpl::Instance()->MainThread()->AddInternalTask(
        TaskPriority::Internal, "", PlainCallbackTask(std::bind(cb, args...)));
  };
}

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_CORE_JS_MANAGER_IMPL_H_
