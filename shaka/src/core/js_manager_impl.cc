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

#include "src/core/js_manager_impl.h"

#include "src/mapping/convert_js.h"
#include "src/mapping/js_engine.h"
#include "src/util/clock.h"
#include "src/util/file_system.h"

namespace shaka {

using std::placeholders::_1;

JsManagerImpl::JsManagerImpl(const JsManager::StartupOptions& options)
    : startup_options_(options),
      event_loop_(std::bind(&JsManagerImpl::EventThreadWrapper, this, _1),
                  &util::Clock::Instance, /* is_worker */ false) {}

JsManagerImpl::~JsManagerImpl() {
  Stop();
}

void JsManagerImpl::Trace(memory::HeapTracer* tracer) const {
  tracer->Trace(&event_loop_);
}

std::string JsManagerImpl::GetPathForStaticFile(const std::string& file) const {
  return util::FileSystem::GetPathForStaticFile(
      startup_options_.static_data_dir,
      startup_options_.is_static_relative_to_bundle, file);
}

std::string JsManagerImpl::GetPathForDynamicFile(
    const std::string& file) const {
  return util::FileSystem::GetPathForDynamicFile(
      startup_options_.dynamic_data_dir, file);
}

void JsManagerImpl::WaitUntilFinished() {
  if (event_loop_.is_running() && event_loop_.HasPendingWork()) {
    event_loop_.WaitUntilFinished();
  }
}

std::shared_ptr<ThreadEvent<bool>> JsManagerImpl::RunScript(
    const std::string& path) {
  // Script must be executed on the event thread.
  CHECK(event_loop_.is_running());
  auto callback = std::bind(
      static_cast<bool (*)(const std::string&)>(&shaka::RunScript), path);
  return event_loop_.AddInternalTask(TaskPriority::Immediate, "RunScript",
                                     PlainCallbackTask(callback));
}

std::shared_ptr<ThreadEvent<bool>> JsManagerImpl::RunScript(
    const std::string& path, const uint8_t* data, size_t data_size) {
  // Script must be executed on the event thread.
  CHECK(event_loop_.is_running());
  auto* run_script =
      static_cast<bool (*)(const std::string&, const uint8_t*, size_t)>(
          &shaka::RunScript);
  auto callback = std::bind(run_script, path, data, data_size);
  return event_loop_.AddInternalTask(TaskPriority::Immediate, "RunScript",
                                     PlainCallbackTask(callback));
}

void JsManagerImpl::EventThreadWrapper(TaskRunner::RunLoop run_loop) {
  JsEngine engine;

  {
    JsEngine::SetupContext setup;
#ifdef USING_V8
    engine.isolate()->SetEmbedderHeapTracer(&v8_heap_tracer_);
#endif

    Environment env;
    env.Install();

    run_loop();

    network_thread_.Stop();
    tracker_.Dispose();
  }
}

}  // namespace shaka
