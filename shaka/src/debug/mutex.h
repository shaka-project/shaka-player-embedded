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

#ifndef SHAKA_EMBEDDED_DEBUG_MUTEX_H_
#define SHAKA_EMBEDDED_DEBUG_MUTEX_H_

#include <glog/logging.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

#include "src/debug/waitable.h"
#include "src/debug/waiting_tracker.h"
#include "src/util/shared_lock.h"

namespace shaka {

/**
 * A wrapper around a mutex.  This exists to provide debug information and
 * to track deadlocks.
 *
 * This implements the Mutex (and Lockable) concept so it can be used in
 * std::unique_lock<T>.  It can also be used in std::condition_variable_any, but
 * it is suggested to use a ThreadEvent instead (to track deadlocks).
 *
 * If _Mutex implements the SharedMutex concept, so does this.
 */
template <typename _Mutex>
class DebugMutex : public Waitable {
 public:
  explicit DebugMutex(const std::string& name)
    : Waitable(name), locked_by_(std::thread::id()) {}
  ~DebugMutex() override {
    CHECK_EQ(locked_by_, std::thread::id())
        << "Attempt to destroy locked mutex.";
  }

  std::thread::id GetProvider() const override {
    return locked_by_;
  }

  void lock() {
    CHECK(!holds_shared_lock())
        << "Cannot hold shared and unique lock at once.";
    CHECK_NE(locked_by_, std::this_thread::get_id())
        << "This isn't a recursive mutex.";
#ifdef DEBUG_DEADLOCKS
    auto scope = WaitingTracker::ThreadWaiting(this);
    // TODO: Add tracking to detect deadlocks with readers; this only works with
    // deadlocks for the exclusive lock.
#endif

    mutex_.lock();

    locked_by_ = std::this_thread::get_id();
  }

  bool try_lock() {
    CHECK(!holds_shared_lock()) << "Cannot hold shared an unique lock at once.";
    CHECK_NE(locked_by_, std::this_thread::get_id())
        << "This isn't a recursive mutex.";

    bool ret = mutex_.try_lock();

    if (ret)
      locked_by_ = std::this_thread::get_id();

    return ret;
  }

  void unlock() {
    CHECK_EQ(locked_by_, std::this_thread::get_id())
        << "Attempt to unlock from wrong thread.";
    locked_by_ = std::thread::id();

    mutex_.unlock();
  }


  void lock_shared() {
    CHECK(!holds_shared_lock()) << "This isn't a recursive mutex.";
    CHECK_NE(locked_by_, std::this_thread::get_id())
        << "Cannot get shared lock with exclusive lock held.";
    // TODO: Detect deadlocks with shared usage.  We can't use the same paths
    // for the exclusive lock because there can be multiple readers and it could
    // report a false-positive.

    mutex_.lock_shared();

    add_shared_lock();
  }

  bool try_lock_shared() {
    CHECK(!holds_shared_lock()) << "This isn't a recursive mutex.";
    CHECK_NE(locked_by_, std::this_thread::get_id())
        << "Cannot get shared lock with exclusive lock held.";

    bool ret = mutex_.try_lock();

    if (ret)
      add_shared_lock();

    return ret;
  }

  void unlock_shared() {
    CHECK(holds_shared_lock()) << "Attempt to unlock from wrong thread.";
    CHECK_EQ(locked_by_, std::thread::id())
        << "Must call unlock() before calling unlock_shared() when upgrading.";
    remove_shared_lock();
    mutex_.unlock_shared();
  }

 private:
  bool holds_shared_lock() {
    std::unique_lock<std::mutex> lock(shared_locked_by_lock_);
    return shared_locked_by_.count(std::this_thread::get_id()) > 0;
  }

  void add_shared_lock() {
    std::unique_lock<std::mutex> lock(shared_locked_by_lock_);
    shared_locked_by_.insert(std::this_thread::get_id());
  }

  void remove_shared_lock() {
    std::unique_lock<std::mutex> lock(shared_locked_by_lock_);
    shared_locked_by_.erase(std::this_thread::get_id());
  }
  _Mutex mutex_;
  std::atomic<std::thread::id> locked_by_;
  std::atomic<bool> is_upgrading_{false};

  std::mutex shared_locked_by_lock_;
  std::unordered_set<std::thread::id> shared_locked_by_;
};

#ifdef DEBUG_DEADLOCKS
using Mutex = DebugMutex<std::mutex>;
using SharedMutex = DebugMutex<util::shared_mutex>;
#else
class Mutex final : public std::mutex {
 public:
  explicit Mutex(const std::string&) {}
};
class SharedMutex final : public util::shared_mutex {
 public:
  explicit SharedMutex(const std::string&) {}
};
#endif

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_DEBUG_MUTEX_H_
