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

#ifndef SHAKA_EMBEDDED_DEBUG_THREAD_EVENT_H_
#define SHAKA_EMBEDDED_DEBUG_THREAD_EVENT_H_

#include <glog/logging.h>

#include <atomic>
#include <future>
#include <string>
#include <utility>

#include "src/debug/waitable.h"
#include "src/debug/waiting_tracker.h"
#include "src/util/utils.h"

namespace shaka {

class Thread;

/**
 * A non-templated base class of ThreadEvent.  This exists to allow generic
 * handling of waiting for events independent of the type of the data.
 */
class ThreadEventBase : public Waitable {
 public:
  explicit ThreadEventBase(const std::string& name);
  ~ThreadEventBase() override;

  /** @return The thread providing this event, or nullptr if not set yet. */
  std::thread::id GetProvider() const override;

  /**
   * Sets which thread will be providing this event.  This must be called
   * exactly once.
   */
  void SetProvider(Thread* thread) {
    provider_.store(thread, std::memory_order_release);
#ifdef DEBUG_DEADLOCKS
    WaitingTracker::UpdateProvider(this);
#endif
  }

 private:
  const std::string name_;
  std::atomic<Thread*> provider_;
};

/**
 * Describes something that needs to happen on a different thread.  One (or
 * more) threads will wait for this event; another thread will be "providing"
 * the event.  The event will only complete on the providing thread.
 *
 * This object is internally thread-safe.  To safely manage memory, it is
 * suggested to use std::shared_ptr<T> to share this object.
 */
template <typename T>
class ThreadEvent final : public ThreadEventBase {
 public:
  explicit ThreadEvent(const std::string& name)
      : ThreadEventBase(name), future_(promise_.get_future().share()) {}
  ~ThreadEvent() override {}

  T GetValue() {
    std::shared_future<T> future;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      future = future_;
    }

#ifdef DEBUG_DEADLOCKS
    auto scope = WaitingTracker::ThreadWaiting(this);
#endif
    return future.get();
  }

  /**
   * Gets the future that is used for this event.  It is not advised to use this
   * as this won't include deadlock detection.
   */
  std::shared_future<T> future() {
    std::unique_lock<std::mutex> lock(mutex_);
    return future_;
  }

  /**
   * Resets this object, unlocks the given lock, and waits for this event to
   * get another signal from another thread.  This is similar to how the wait()
   * method works on a std::condition_variable.
   *
   * When this returns, the lock will be acquired again.
   */
  template <typename _Mutex>
  T ResetAndWaitWhileUnlocked(std::unique_lock<_Mutex>& lock) {  // NOLINT
    DCHECK(lock.owns_lock());

    std::shared_future<T> future;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      ResetInternal();
      future = future_;
    }

    util::Unlocker<_Mutex> unlock(&lock);
#ifdef DEBUG_DEADLOCKS
    auto scope = WaitingTracker::ThreadWaiting(this);
#endif
    return future.get();
  }

  //@{
  /**
   * Sets the result of the event.  This can only be called once per-Reset().
   * Calling Reset() will allow this to be called again.
   */
  void SignalAll() {
    CHECK(SignalAllIfNotSet());
  }
  template <typename U>
  void SignalAll(U&& value) {
    CHECK(SignalAllIfNotSet(std::forward<U>(value)));
  }
  // @}

  //@{
  /**
   * Sets the result of the event if it has not already been set.  If this has
   * already been set, this has no effect.
   * @return True if a signal was set, false if we were already set.
   */
  bool SignalAllIfNotSet() {
    // Don't signal with the lock held since once we signal, this object may be
    // destroyed by thread we woke up.
    std::promise<void> p;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (is_set_)
        return false;
      is_set_ = true;
      p = std::move(promise_);
    }
    p.set_value();
    return true;
  }
  template <typename U>
  bool SignalAllIfNotSet(U&& value) {
    std::promise<T> p;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (is_set_)
        return false;
      is_set_ = true;
      p = std::move(promise_);
    }
    p.set_value(std::forward<U>(value));
    return true;
  }
  // @}

  /**
   * Resets the internal future so it can be used again.  This asserts that
   * there are no waiting threads, so it is important to call SignalAll() before
   * this so any waiting threads will get the value.
   *
   * Calls to get() after this call will need to wait until the next call to
   * SignalAll().
   */
  void Reset() {
    std::unique_lock<std::mutex> lock(mutex_);
    ResetInternal();
  }

 private:
  void ResetInternal() {
#ifdef DEBUG_DEADLOCKS
    // Removing is the same as resetting.
    WaitingTracker::RemoveWaitable(this);
#endif

    // When a std::promise is destroyed and its value hasn't been set, the
    // promise should have its exception set to std::broken_promise.  Some
    // versions of the C++ library can't do this with no exceptions.  Similarly,
    // we can't set the exception to a value ourselves; but we can set the
    // exception to the nullptr.
    if (!is_set_)
      promise_.set_exception(std::exception_ptr());

    is_set_ = false;
    promise_ = std::promise<T>();
    future_ = promise_.get_future().share();
  }

  std::mutex mutex_;
  std::promise<T> promise_;
  std::shared_future<T> future_;
  bool is_set_ = false;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_DEBUG_THREAD_EVENT_H_
