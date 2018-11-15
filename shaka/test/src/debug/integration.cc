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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/debug/mutex.h"
#include "src/debug/thread.h"
#include "src/debug/thread_event.h"
#include "src/test/test_utils.h"

namespace shaka {

namespace {

#if defined(DEBUG_DEADLOCKS) && defined(GTEST_HAS_DEATH_TEST)
/**
 * Defines a new gtest test that expects that the body that follows this will
 * result in a program crash.  This is done using gtest death tests.
 * See:
 * https://github.com/google/googletest/blob/master/googletest/docs/AdvancedGuide.md#how-to-write-a-death-test
 */
#  define DEFINE_DEATH_TEST(cat, name, regex)     \
    void cat##name##TestBody();                   \
    TEST(cat, name) {                             \
      ASSERT_DEATH(cat##name##TestBody(), regex); \
    }                                             \
    void cat##name##TestBody()

#endif

}  // namespace


TEST(MutexTest, WorksWithUniqueLock) {
  Mutex mutex("");

  {
    std::unique_lock<Mutex> lock1(mutex);
    EXPECT_EQ(0, 0);
  }

  auto func = [&]() {
    std::unique_lock<Mutex> lock2(mutex);
    EXPECT_EQ(1, 1);
  };
  func();
}

TEST(MutexTest, AllowsMultipleWaitingThreads) {
  // This ensures the deadlock code doesn't trigger just because threads are
  // waiting.
  Mutex mutex("");
  mutex.lock();

  std::thread thread1([&]() {
    std::unique_lock<Mutex> lock(mutex);
    EXPECT_EQ(1, 1);
  });
  std::thread thread2([&]() {
    std::unique_lock<Mutex> lock(mutex);
    EXPECT_EQ(1, 1);
  });
  std::thread thread3([&]() {
    std::unique_lock<Mutex> lock(mutex);
    EXPECT_EQ(1, 1);
  });

  mutex.unlock();
  thread1.join();
  thread2.join();
  thread3.join();
}

#if defined(DEBUG_DEADLOCKS) && defined(GTEST_HAS_DEATH_TEST)
DEFINE_DEATH_TEST(MutexDeathTest, DontAllowRecursion, "recursive mutex") {
  Mutex mutex("");

  std::unique_lock<Mutex> lock1(mutex);
  std::unique_lock<Mutex> lock2(mutex);
}

DEFINE_DEATH_TEST(MutexDeathTest, DestroyLockedMutex, "destroy locked") {
  Mutex mutex("");
  mutex.lock();
}

DEFINE_DEATH_TEST(DeadlockDeathTest, DetectsMutexDeadlocks,
                  "Deadlock detected") {
  Mutex mutex1("m1");
  Mutex mutex2("m2");

  std::thread thread([&]() {
    std::unique_lock<Mutex> lock1(mutex1);
    WaitUntilBlocking(&mutex2);
    std::unique_lock<Mutex> lock2(mutex2);
  });

  std::unique_lock<Mutex> lock1(mutex2);
  WaitUntilBlocking(&mutex1);
  std::unique_lock<Mutex> lock2(mutex1);

  thread.join();
}

DEFINE_DEATH_TEST(DeadlockDeathTest, DetectsThreadEventDeadlocks,
                  "Deadlock detected") {
  ThreadEvent<void> event1("e1");
  ThreadEvent<void> event2("e2");

  Thread t1("t1", [&]() {
    usleep(50);
    event2.GetValue();
  });
  Thread t2("t2", [&]() {
    usleep(50);
    event1.GetValue();
  });

  event1.SetProvider(&t1);
  event2.SetProvider(&t2);
  t1.join();
  t2.join();
}

DEFINE_DEATH_TEST(DeadlockDeathTest, DetectsCombinedDeadlocks,
                  "Deadlock detected") {
  ThreadEvent<void> event1("e1");
  ThreadEvent<void> event2("e2");
  Mutex mutex1("m1");
  Mutex mutex2("m2");

  Thread t1("t1", [&]() {
    std::unique_lock<Mutex> lock(mutex1);
    event1.GetValue();
  });
  Thread t2("t2", [&]() {
    std::unique_lock<Mutex> lock(mutex2);
    event2.GetValue();
  });

  Thread t3("t3", [&]() {
    WaitUntilBlocking(&mutex2);
    std::unique_lock<Mutex> lock(mutex2);
    LOG(FATAL) << "Should not acquire lock";
  });
  Thread t4("t4", [&]() {
    WaitUntilBlocking(&mutex1);
    std::unique_lock<Mutex> lock(mutex1);
    LOG(FATAL) << "Should not acquire lock";
  });

  event1.SetProvider(&t3);
  event2.SetProvider(&t4);
  t1.join();
  t2.join();
  t3.join();
  t4.join();
}

DEFINE_DEATH_TEST(DeadlockDeathTest, DetectsThreadEndWhileWaiting,
                  "Waiting.*thread.*exited") {
  ThreadEvent<void> event("e");

  Thread thread("thread", [&]() { usleep(500); });
  event.SetProvider(&thread);

  event.GetValue();
}

DEFINE_DEATH_TEST(DeadlockDeathTest, DetectsThreadEndBeforeWaiting,
                  "Waiting.*thread.*exited") {
  ThreadEvent<void> event("e");

  Thread thread("thread", [&]() {});
  event.SetProvider(&thread);

  thread.join();
  event.GetValue();
}

#endif

}  // namespace shaka
