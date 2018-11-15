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

#ifndef SHAKA_EMBEDDED_TEST_TEST_UTILS_H_
#define SHAKA_EMBEDDED_TEST_TEST_UTILS_H_

#include "src/util/clock.h"

namespace shaka {

/** The number of milliseconds to wait for a timeout. */
constexpr const uint64_t kTimeout = 100;

/**
 * Tries to lock the given Mutex.  If it can lock it, this spins until another
 * thread acquires the Mutex.  Once this returns, the Mutex *should* be locked
 * by another thread.
 */
template <typename _Mutex>
void WaitUntilBlocking(_Mutex* mutex) {
  while (mutex->try_lock()) {
    mutex->unlock();
    util::Clock::Instance.SleepSeconds(0.001);
  }
}

/**
 * Tries to lock the given Mutex in shared mode.  If it can lock it, this spins
 * until another thread acquires the Mutex.  Once this returns, the Mutex
 * *should* be locked by another thread.
 */
template <typename _Mutex>
void WaitUntilBlockingShared(_Mutex* mutex) {
  while (mutex->try_lock_shared()) {
    mutex->unlock_shared();
    util::Clock::Instance.SleepSeconds(0.001);
  }
}

/**
 * Waits until the given predicate returns true, or a timeout occurs.
 * @return True if the condition was hit, false if this timed-out.
 */
template <typename T>
bool WaitUntilOrTimeout(T callback) {
  util::Clock clock;
  const uint64_t start = clock.GetMonotonicTime();
  while (!callback()) {
    if (clock.GetMonotonicTime() - start >= kTimeout)
      return false;
    clock.SleepSeconds(0.001);
  }
  return true;
}

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_TEST_TEST_UTILS_H_
