// Copyright 2017 Google LLC
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

#ifndef SHAKA_EMBEDDED_UTIL_CLOCK_H_
#define SHAKA_EMBEDDED_UTIL_CLOCK_H_

#include <stdint.h>

namespace shaka {
namespace util {

/** Used to get the current system time.  This can be overridden by tests. */
class Clock {
 public:
  virtual ~Clock() {}

  /** Contains a static instance of the clock. */
  static const Clock Instance;

  /**
   * @return The current time, in milliseconds.  The value is not specific, but
   *   is guaranteed to be increasing over the course of the program.
   */
  virtual uint64_t GetMonotonicTime() const;

  /** @return The current wall-clock time, in milliseconds. */
  virtual uint64_t GetEpochTime() const;

  /** Sleeps for the given number of seconds. */
  virtual void SleepSeconds(double seconds) const;
};

}  // namespace util
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTIL_CLOCK_H_
