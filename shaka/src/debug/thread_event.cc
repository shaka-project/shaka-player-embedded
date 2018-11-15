// Copyright 2018 Google LLC
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

#include "src/debug/thread_event.h"

#include "src/debug/thread.h"

namespace shaka {

ThreadEventBase::ThreadEventBase(const std::string& name)
    : Waitable(name), provider_(nullptr) {}

ThreadEventBase::~ThreadEventBase() {}

std::thread::id ThreadEventBase::GetProvider() const {
  Thread* thread = provider_.load(std::memory_order_acquire);
  if (thread && thread->get_id() == std::thread::id()) {
    // Once a thread exits, its thread id gets reset.
    LOG(FATAL) << "Waiting on an event whose provider thread has exited: "
               << name();
  }
  return thread ? thread->get_id() : std::thread::id();
}

}  // namespace shaka
