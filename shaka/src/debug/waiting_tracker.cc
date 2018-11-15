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

#include "src/debug/waiting_tracker.h"

#include <glog/logging.h>

#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "src/debug/mutex.h"
#include "src/debug/thread.h"
#include "src/debug/thread_event.h"
#include "src/debug/waitable.h"
#include "src/util/macros.h"

namespace shaka {

#ifdef DEBUG_DEADLOCKS

namespace {

BEGIN_ALLOW_COMPLEX_STATICS
std::mutex global_mutex_;
std::unordered_map<std::thread::id, const Waitable*> waiting_threads_;
std::unordered_map<std::thread::id, const Thread*> all_threads_;
END_ALLOW_COMPLEX_STATICS

std::string ThreadName(std::thread::id id) {
  std::stringstream ss;
  if (all_threads_.count(id) > 0)
    ss << all_threads_.at(id)->name() << " (" << id << ")";
  else
    ss << id;
  return ss.str();
}

/**
 * Detects whether the given thread belongs to a deadlock cycle.  If it does,
 * this prints out the threads/conditions involved in the deadlock and crashes.
 */
void DetectDeadlock(const Waitable* start, std::thread::id start_thread_id) {
  std::stringstream trace;
  std::unordered_set<std::thread::id> seen;
  seen.insert(start_thread_id);

  // If a thread is waiting on something, find the thread that will provide it.
  // If that produces a cycle, then we are deadlocked.
  std::thread::id prev_thread_id = start_thread_id;
  const Waitable* waiting_on = start;
  int i = 0;
  while (true) {
    const std::thread::id provider = waiting_on->GetProvider();
    if (provider == std::thread::id())
      break;

    // We can be called between setting the provider and indicating to this code
    // it is done waiting.
    if (provider == prev_thread_id)
      break;

    // (0) FooThread -> "bar" provided by: (1)
    trace << "(" << i << ") " << ThreadName(prev_thread_id) << " -> \""
          << waiting_on->name() << "\" provided by"
          << ": (" << (provider == start_thread_id ? 0 : i + 1) << ")\n";

    if (seen.count(provider)) {
      LOG(FATAL)
          << "Deadlock detected:\n"
          << "(i) thread name (id) -> waiting on\n"
          << "--------------------------------------------------------------\n"
          << trace.str()
          << "--------------------------------------------------------------";
    }

    if (waiting_threads_.count(provider) == 0)
      break;
    seen.insert(provider);
    prev_thread_id = provider;
    waiting_on = waiting_threads_.at(provider);
    i++;
  }
}

}  // namespace

WaitingTracker::TrackerScope::TrackerScope() : exec_(true) {}

WaitingTracker::TrackerScope::TrackerScope(TrackerScope&& other) : exec_(true) {
  other.exec_ = false;
}

WaitingTracker::TrackerScope& WaitingTracker::TrackerScope::operator=(
    TrackerScope&& other) {
  other.exec_ = false;
  exec_ = true;
  return *this;
}

WaitingTracker::TrackerScope::~TrackerScope() {
  if (exec_) {
    std::unique_lock<std::mutex> lock(global_mutex_);
    CHECK_EQ(waiting_threads_.count(std::this_thread::get_id()), 1u);
    waiting_threads_.erase(std::this_thread::get_id());
  }
}


// static
void WaitingTracker::AddThread(const Thread* thread) {
  std::unique_lock<std::mutex> lock(global_mutex_);
  CHECK_EQ(all_threads_.count(thread->get_original_id()), 0u);
  all_threads_[thread->get_original_id()] = thread;
}

// static
void WaitingTracker::RemoveThread(const Thread* thread) {
  std::unique_lock<std::mutex> lock(global_mutex_);
  CHECK_EQ(all_threads_.count(thread->get_original_id()), 1u);
  CHECK_EQ(waiting_threads_.count(thread->get_original_id()), 0u)
      << "Attempt to destroy thread that is waiting.";
  all_threads_.erase(thread->get_original_id());
}

// static
void WaitingTracker::RemoveWaitable(const Waitable* waiting_on) {
  std::unique_lock<std::mutex> lock(global_mutex_);
  for (auto& pair : waiting_threads_) {
    if (pair.second == waiting_on)
      LOG(FATAL) << "Attempt to destroy an object someone is waiting for.";
  }
}

// static
void WaitingTracker::ThreadExit() {
  std::unique_lock<std::mutex> lock(global_mutex_);
  for (auto& pair : waiting_threads_) {
    const std::thread::id provider = pair.second->GetProvider();
    if (provider == std::this_thread::get_id()) {
      LOG(FATAL) << "Waiting on an event whose provider thread has exited: "
                 << pair.second->name();
    }
  }
}

// static
void WaitingTracker::UpdateProvider(const Waitable* waiting_on) {
  std::unique_lock<std::mutex> lock(global_mutex_);

  const std::thread::id provider = waiting_on->GetProvider();
  if (waiting_threads_.count(provider))
    DetectDeadlock(waiting_threads_.at(provider), provider);
}

// static
WaitingTracker::TrackerScope WaitingTracker::ThreadWaiting(
    const Waitable* waiting_on) {
  std::unique_lock<std::mutex> lock(global_mutex_);

  const std::thread::id this_thread_id = std::this_thread::get_id();
  DetectDeadlock(waiting_on, this_thread_id);

  CHECK_EQ(waiting_threads_.count(this_thread_id), 0u)
      << "Somehow waiting on two conditions at once.";
  waiting_threads_[this_thread_id] = waiting_on;
  return TrackerScope();
}
#endif

}  // namespace shaka
