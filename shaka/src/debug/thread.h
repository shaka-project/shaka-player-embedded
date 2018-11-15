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

#ifndef SHAKA_EMBEDDED_DEBUG_THREAD_H_
#define SHAKA_EMBEDDED_DEBUG_THREAD_H_

#include <functional>
#include <string>
#include <thread>

namespace shaka {

class Thread final {
 public:
  Thread(const std::string& name, std::function<void()> callback);
  Thread(const Thread&) = delete;
  Thread(Thread&&) = delete;
  ~Thread();

  /** @return The name of the thread. */
  std::string name() const {
    return name_;
  }

  /** @return Whether you can call join() on the thread. */
  bool joinable() const {
    return thread_.joinable();
  }

  /** @return The unique thread ID of this thread. */
  std::thread::id get_id() const {
    return thread_.get_id();
  }

#ifdef DEBUG_DEADLOCKS
  /**
   * @return The ID of the thread when it was created.  If the thread exits,
   *   it is technically possible for the ID to be reused.
   */
  std::thread::id get_original_id() const {
    return original_id_;
  }
#endif

  void join() {
    thread_.join();
  }

 private:
  const std::string name_;
  std::thread thread_;
#ifdef DEBUG_DEADLOCKS
  std::thread::id original_id_;
#endif
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_DEBUG_THREAD_H_
