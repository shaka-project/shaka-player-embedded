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

#include "src/debug/thread.h"

#include <glog/logging.h>

#include <utility>

#include "src/debug/waiting_tracker.h"
#include "src/util/utils.h"

namespace shaka {

namespace {

void ThreadMain(const std::string& name, std::function<void()> callback) {
#if defined(OS_MAC) || defined(OS_IOS)
  pthread_setname_np(name.c_str());
#elif defined(OS_POSIX)
  pthread_setname_np(pthread_self(), name.c_str());
#else
#  error "Not implemented for Windows"
#endif

#ifdef DEBUG_DEADLOCKS
  util::Finally scope(&WaitingTracker::ThreadExit);
#endif
  callback();
}

}  // namespace

Thread::Thread(const std::string& name, std::function<void()> callback)
    : name_(name), thread_(&ThreadMain, name, std::move(callback)) {
  DCHECK_LT(name.size(), 16u) << "Name too long: " << name;
#ifdef DEBUG_DEADLOCKS
  original_id_ = thread_.get_id();
  WaitingTracker::AddThread(this);
#endif
}

Thread::~Thread() {
  DCHECK(!thread_.joinable());
#ifdef DEBUG_DEADLOCKS
  WaitingTracker::RemoveThread(this);
#endif
}

}  // namespace shaka
