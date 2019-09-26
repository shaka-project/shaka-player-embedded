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

#include "shaka/js_manager.h"

#include "src/core/js_manager_impl.h"

namespace shaka {

JsManager::JsManager() : impl_(new JsManagerImpl(StartupOptions())) {}
JsManager::JsManager(const StartupOptions& options)
    : impl_(new JsManagerImpl(options)) {}
JsManager::~JsManager() {}

JsManager::JsManager(JsManager&&) = default;
JsManager& JsManager::operator=(JsManager&&) = default;


void JsManager::Stop() {
  impl_->Stop();
}

void JsManager::WaitUntilFinished() {
  impl_->WaitUntilFinished();
}

AsyncResults<void> JsManager::RunScript(const std::string& path) {
  auto run_future = impl_->RunScript(path)->future();
  // This creates a std::future that will invoke the given method when the
  // wait()/get() method is called.  This exists to convert the
  // std::future<bool> that is returned into a
  // std::future<variant<monostate, Error>> that AsyncResults expects.
  // TODO: Find a better way to do this.
  auto future = std::async(std::launch ::deferred,
                           [run_future]() -> variant<monostate, Error> {
                             if (run_future.get())
                               return monostate();

                             return Error("Unknown script error");
                           });

  return future.share();
}

}  // namespace shaka
