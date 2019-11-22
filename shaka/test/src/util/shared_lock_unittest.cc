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

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "src/test/test_utils.h"

namespace shaka {
namespace util {

namespace {

template <typename T>
std::vector<std::thread> make_n_threads(size_t count, T cb) {
  std::vector<std::thread> ret;
  ret.reserve(count);
  while (count-- > 0) {
    ret.emplace_back(cb);
  }
  return ret;
}

void join_all(std::vector<std::thread>* threads) {
  for (auto& t : *threads)
    t.join();
}

}  // namespace

TEST(SharedLock, CanBeUsedWithUniqueLock) {
  shared_mutex mutex;
  {
    std::unique_lock<shared_mutex> l(mutex);
    EXPECT_EQ(1, 1);
  }
}

TEST(SharedLock, CanTryLock) {
  shared_mutex mutex;
  std::unique_lock<shared_mutex> l(mutex);

  std::thread t([&]() { EXPECT_FALSE(mutex.try_lock()); });
  t.join();
}

TEST(SharedLock, CanTryLockShared) {
  shared_mutex mutex;
  std::unique_lock<shared_mutex> l(mutex);

  std::thread t([&]() {
    // Since we hold the exclusive lock, this will fail.
    EXPECT_FALSE(mutex.try_lock_shared());
  });
  t.join();
}

TEST(SharedLock, IsExclusiveLock) {
  shared_mutex mutex;
  std::atomic_flag flag = ATOMIC_FLAG_INIT;

  std::thread a([&]() {
    std::unique_lock<shared_mutex> l(mutex);
    EXPECT_FALSE(flag.test_and_set());
    usleep(1000);
    flag.clear();
  });
  std::thread b([&]() {
    usleep(100);

    std::unique_lock<shared_mutex> l(mutex);
    EXPECT_FALSE(flag.test_and_set());
    usleep(1000);
    flag.clear();
  });

  a.join();
  b.join();
}

TEST(SharedLock, AllowsMultipleReaders) {
  shared_mutex mutex;
  std::atomic<int> readers{0};
  std::atomic<int> max_readers{0};

  constexpr const int kThreadCount = 5;
  std::vector<std::thread> threads = make_n_threads(kThreadCount, [&]() {
    shared_lock<shared_mutex> l(mutex);
    const int count = ++readers;

    // Atomically set the max value.  Each step will try to update the value; if
    // the value has changed since the read, |last_max| will be updated and the
    // loop will step again.
    int last_max = max_readers;
    while (
        !max_readers.compare_exchange_weak(last_max, std::max(last_max, count)))
      ;

    WaitUntilOrTimeout([&]() { return max_readers >= kThreadCount; });
    --readers;
  });
  join_all(&threads);

  EXPECT_EQ(0, readers);
  EXPECT_EQ(kThreadCount, max_readers);
}

TEST(SharedLock, ReaderBlocksWriters) {
  shared_mutex mutex;
  std::atomic<int> reader_count{0};
  std::atomic<bool> waiting_for_write{false};

  std::vector<std::thread> readers = make_n_threads(3, [&]() {
    shared_lock<shared_mutex> l(mutex);
    reader_count++;
    EXPECT_TRUE(
        WaitUntilOrTimeout([&]() -> bool { return waiting_for_write; }));
    usleep(1000);
    reader_count--;
  });
  std::thread writer([&]() {
    ASSERT_TRUE(WaitUntilOrTimeout([&]() { return reader_count > 0; }));
    ASSERT_FALSE(mutex.try_lock());
    waiting_for_write = true;

    std::unique_lock<shared_mutex> l(mutex);
    EXPECT_EQ(0, reader_count);
    usleep(1000);
  });

  join_all(&readers);
  writer.join();
}

}  // namespace util
}  // namespace shaka
