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

#ifndef SHAKA_EMBEDDED_UTIL_SHARED_LOCK_H_
#define SHAKA_EMBEDDED_UTIL_SHARED_LOCK_H_

#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <utility>

#include "src/util/macros.h"

namespace shaka {
namespace util {

/**
 * A simple implementation of a reader-writer mutex where:
 * - It doesn't handle any scheduling, so it will likely be inefficient.
 * - It is not a recursive mutex, so you cannot use lock() or lock_shared() if
 *   this thread already holds a lock.
 * - It cannot use both lock() and lock_shared() on the same thread at the same
 *   time.
 *
 * You can use std::unique_lock<T> to help locking this in exclusive mode.  For
 * example:
 *
 * \code{cpp}
 *   std::unique_lock<shared_mutex> lock(mutex);
 *   // In exclusive mode.
 * \endcode
 *
 * Or you can use shared_lock to lock in shared mode:
 *
 * \code{cpp}
 *   shared_lock<shared_mutex> lock(mutex);
 *   // In shared mode.
 * \endcode
 *
 * This implements the Lockable, Mutex, and SharedMutex concepts.
 */
class shared_mutex {
 public:
  shared_mutex();
  ~shared_mutex();

  NON_COPYABLE_OR_MOVABLE_TYPE(shared_mutex);

  /** Locks the mutex for exclusive access. */
  void lock() {
    maybe_try_lock(/* only_try */ false);
  }

  /** Tries to lock the mutex for exclusive access in a non-blocking way. */
  bool try_lock() {
    return maybe_try_lock(/* only_try */ true);
  }

  /** Unlocks the mutex from exclusive access. */
  void unlock();


  /** Locks the mutex for shared access. */
  void lock_shared() {
    maybe_try_lock_shared(/* only_try */ false);
  }

  /** Tries to lock the mutex for shared access in a non-blocking way. */
  bool try_lock_shared() {
    return maybe_try_lock_shared(/* only_try */ true);
  }

  /** Unlocks the mutex from shared access. */
  void unlock_shared();

 private:
  bool maybe_try_lock(bool only_try);
  bool maybe_try_lock_shared(bool only_try);

  std::mutex mutex_;
  std::condition_variable signal_;
  uint32_t shared_count_ = 0;
  bool is_exclusive_ = false;
  bool is_exclusive_waiting_ = false;
};


/**
 * Similar to std::unique_lock, this locks the given shared mutex in the shared
 * mode.
 */
template <typename Mutex>
class shared_lock {
 public:
  using mutex_type = Mutex;

  shared_lock() : mutex_(nullptr), owns_lock_(false) {}
  explicit shared_lock(mutex_type& mutex) : mutex_(&mutex) {
    mutex_->lock_shared();
    owns_lock_ = true;
  }
  shared_lock(shared_lock&& other)
      : mutex_(other.mutex_), owns_lock_(other.owns_lock_) {
    other.mutex_ = nullptr;
    other.owns_lock_ = false;
  }

  ~shared_lock() {
    if (owns_lock_)
      mutex_->unlock_shared();
  }

  NON_COPYABLE_TYPE(shared_lock);

  shared_lock& operator=(shared_lock&& other) {
    other.swap(*this);
    return *this;
  }

  operator bool() const { return owns_lock_; }
  bool owns_lock() const { return owns_lock_; }
  mutex_type* mutex() const { return mutex_; }

  void swap(shared_lock& other) {
    swap(mutex_, other.mutex_);
    swap(owns_lock_, other.owns_lock_);
  }

  mutex_type* release() {
    Mutex* ret = mutex_;
    mutex_ = nullptr;
    owns_lock_ = false;
    return ret;
  }

 private:
  mutex_type* mutex_;
  bool owns_lock_;
};

template <typename Mutex>
void swap(shared_lock<Mutex>& a, shared_lock<Mutex>& b) {
  a.swap(b);
}

}  // namespace util
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTIL_SHARED_LOCK_H_
