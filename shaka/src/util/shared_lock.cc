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

#include "src/util/shared_lock.h"

#include <glog/logging.h>

namespace shaka {
namespace util {

shared_mutex::shared_mutex() {
#ifdef USE_PTHREAD
  CHECK_EQ(pthread_rwlock_init(&lock_, nullptr), 0);
#endif
}

shared_mutex::~shared_mutex() {
#ifdef USE_PTHREAD
  CHECK_EQ(pthread_rwlock_destroy(&lock_), 0);
#else
  DCHECK(!is_exclusive_) << "Trying to destroy a locked mutex";
  DCHECK_EQ(0, shared_count_) << "Trying to destroy a locked mutex";
#endif
}

void shared_mutex::unlock() {
#ifdef USE_PTHREAD
  CHECK_EQ(pthread_rwlock_unlock(&lock_), 0);
#else
  {
    std::unique_lock<std::mutex> lock(mutex_);
    DCHECK(is_exclusive_) << "Trying to unlock an already unlocked mutex";
    DCHECK_EQ(shared_count_, 0) << "Cannot have shared locks in exclusive mode";
    is_exclusive_ = false;
  }
  signal_.notify_all();
#endif
}

void shared_mutex::unlock_shared() {
#ifdef USE_PTHREAD
  CHECK_EQ(pthread_rwlock_unlock(&lock_), 0);
#else
  {
    std::unique_lock<std::mutex> lock(mutex_);
    DCHECK(!is_exclusive_) << "Cannot hold unique lock with shared lock";
    DCHECK_GT(shared_count_, 0) << "Trying to unlock an already unlocked mutex";
    shared_count_--;
  }
  signal_.notify_all();
#endif
}

bool shared_mutex::maybe_try_lock(bool only_try) {
#ifdef USE_PTHREAD
  const int code = only_try ? pthread_rwlock_trywrlock(&lock_)
                            : pthread_rwlock_wrlock(&lock_);
  CHECK(code == 0 || code == EBUSY);
  return code == 0;
#else
  // Note this lock is only held transitively, so it shouldn't block for long.
  std::unique_lock<std::mutex> lock(mutex_);
  while (is_exclusive_ || shared_count_ > 0) {
    if (only_try)
      return false;
    is_exclusive_waiting_ = true;
    signal_.wait(lock);
  }
  is_exclusive_ = true;
  is_exclusive_waiting_ = false;
  return true;
#endif
}

bool shared_mutex::maybe_try_lock_shared(bool only_try) {
#ifdef USE_PTHREAD
  const int code = only_try ? pthread_rwlock_tryrdlock(&lock_)
                            : pthread_rwlock_rdlock(&lock_);
  CHECK(code == 0 || code == EBUSY);
  return code == 0;
#else
  // Note this lock is only held transitively, so it shouldn't block for long.
  std::unique_lock<std::mutex> lock(mutex_);

  // Wait if there is an exclusive lock waiting.  This ensures that if there
  // are a bunch of readers, a writer can still get in.
  while (is_exclusive_ || is_exclusive_waiting_) {
    if (only_try)
      return false;
    signal_.wait(lock);
  }
  shared_count_++;
  return true;
#endif
}

}  // namespace util
}  // namespace shaka
