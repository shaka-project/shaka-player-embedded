// Copyright 2016 Google LLC
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

#ifndef SHAKA_EMBEDDED_UTIL_UTILS_H_
#define SHAKA_EMBEDDED_UTIL_UTILS_H_

#include <glog/logging.h>
#include <stdarg.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/util/macros.h"

namespace shaka {
namespace util {

/**
 * A helper type that accepts a function in the constructor and calls that
 * method in the destructor.  This can be used to execute code whether or not
 * an exception has been thrown.
 */
struct Finally {
  explicit Finally(std::function<void()> call);
  Finally(const Finally&) = delete;
  Finally(Finally&&) = delete;
  ~Finally();

  std::function<void()> call_;
};
inline Finally::Finally(std::function<void()> call) : call_(std::move(call)) {}
inline Finally::~Finally() {
  call_();
}


/**
 * A helper that unlocks the given Lock and in the destructor, locks it again.
 */
template <typename _Mutex>
struct Unlocker {
  explicit Unlocker(std::unique_lock<_Mutex>* lock) : lock_(lock) {
    DCHECK(lock_->owns_lock());
    lock_->unlock();
  }
  ~Unlocker() {
    lock_->lock();
  }

  NON_COPYABLE_OR_MOVABLE_TYPE(Unlocker);

  std::unique_lock<_Mutex>* lock_;
};


PRINTF_FORMAT(1, 2)
std::string StringPrintf(const char* format, ...);
PRINTF_FORMAT(1, 0)
std::string StringPrintfV(const char* format, va_list va);

std::vector<std::string> StringSplit(const std::string& source, char split_on);

std::string ToAsciiLower(const std::string& source);
std::string TrimAsciiWhitespace(const std::string& source);

std::string ToHexString(const uint8_t* data, size_t data_size);

template <typename T>
bool contains(const std::vector<T>& vec, const T& elem) {
  return std::find(vec.begin(), vec.end(), elem) != vec.end();
}
template <typename T>
bool contains(const std::unordered_set<T>& set, const T& elem) {
  return set.count(elem) != 0;
}

}  // namespace util
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTIL_UTILS_H_
