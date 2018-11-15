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

#ifndef SHAKA_EMBEDDED_ASYNC_RESULTS_H_
#define SHAKA_EMBEDDED_ASYNC_RESULTS_H_

#include <chrono>
#include <future>
#include <type_traits>

#include "error.h"
#include "variant.h"

namespace shaka {

/**
 * This represents the results of an asynchronous operation.  This type is
 * similar to a std::future.  This stores either the resulting value of the
 * operation or the Error object that occurred.  AsyncResults<void> don't have
 * results and only store the optional error.
 *
 * Unlike std::future, this will block the destructor until the operation
 * finishes.  This means that if you don't store the results in a variable,
 * the function becomes synchronous.
 *
 * \code{cpp}
 *   // Return value not stored, second load will not run until first finishes.
 *   Player.Load();
 *   Player.Load();
 * \endcode
 *
 * @ingroup player
 */
template <typename T>
class AsyncResults final {
 public:
  using result_type = typename std::conditional<std::is_same<T, void>::value,
                                                monostate, T>::type;
  using variant_type = variant<result_type, Error>;

  /** Creates an empty results object. */
  AsyncResults() {}

  /** Creates an object that will wrap the given std::shared_future. */
  AsyncResults(std::shared_future<variant_type> future) : future_(future) {}

  ~AsyncResults() {
    if (future_.valid())
      future_.wait();
  }

  // \cond Doxygen_Skip
  AsyncResults(AsyncResults&&) = default;
  AsyncResults& operator=(AsyncResults&&) = default;
  AsyncResults(const AsyncResults&) = delete;
  AsyncResults& operator=(const AsyncResults&) = delete;
  // \endcond


  /** Checks if the future refers to a shared state. */
  bool valid() const {
    return future_.valid();
  }

  /** Waits until the results are available. */
  void wait() {
    future_.wait();
  }

  /**
   * Waits for the results to be available, unless the given duration of time
   * passes.
   */
  template <typename Rep, typename Period>
  std::future_status wait_for(
      const std::chrono::duration<Rep, Period>& timeout_duration) const {
    return future_.wait_for(timeout_duration);
  }

  /**
   * Waits for the results to be available, unless it still isn't available
   * when the given time happens.
   */
  template <typename Clock, typename Duration>
  std::future_status wait_until(
      std::chrono::time_point<Clock, Duration>& timeout_time) const {
    return future_.wait_until(timeout_time);
  }


  /**
   * Blocks until the results are available and returns whether this contains
   * an error.
   */
  bool has_error() const {
    return holds_alternative<Error>(future_.get());
  }

  /**
   * Blocks until the results are available and returns the response object.
   * This is only valid if there isn't an error.
   */
  template <typename U = T, typename = typename std::enable_if<
                                !std::is_same<U, void>::value>::type>
  const U& results() const {
    return get<T>(future_.get());
  }

  /**
   * Blocks until the results are available and returns the error object.
   * This is only valid if there is an error.
   */
  const Error& error() const {
    return get<Error>(future_.get());
  }

 private:
  std::shared_future<variant_type> future_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_ASYNC_RESULTS_H_
