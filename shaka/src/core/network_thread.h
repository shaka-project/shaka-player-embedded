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

#ifndef SHAKA_EMBEDDED_CORE_NETWORK_THREAD_H_
#define SHAKA_EMBEDDED_CORE_NETWORK_THREAD_H_

#include <atomic>
#include <vector>

#include "src/core/ref_ptr.h"
#include "src/debug/mutex.h"
#include "src/debug/thread.h"
#include "src/debug/thread_event.h"

typedef void CURLM;

namespace shaka {

namespace js {
class XMLHttpRequest;
}  // namespace js

/**
 * This manages a background thread that runs network requests using CURL.  This
 * uses a CURL multi-handle to make multiple requests concurrently.  As the
 * request happens, the background thread will make calls into the XHR object.
 * The XHR object MUST handle any synchronization required for cross-thread
 * access.
 */
class NetworkThread {
 public:
  NetworkThread();
  ~NetworkThread();

  /** Stops the background thread and joins it. */
  void Stop();

  /** @return Whether the given request is being processed. */
  bool ContainsRequest(RefPtr<js::XMLHttpRequest> request) const;

  /**
   * Adds a request to perform.  The request will be kept alive by this object
   * until the request is complete.  The object MUST NOT be otherwise destroyed
   * until the request is done or is aborted.  This will hold a reference to the
   * object.
   */
  void AddRequest(RefPtr<js::XMLHttpRequest> request);

  /**
   * Aborts a pending request.  Depending on timing, the request may already be
   * done.  Once this method returns, the CURL handle has been removed from
   * this object, so it is safe to free.
   */
  void AbortRequest(RefPtr<js::XMLHttpRequest> request);

 private:
  void ThreadMain();

  mutable Mutex mutex_;
  ThreadEvent<void> cond_;
  std::vector<RefPtr<js::XMLHttpRequest>> requests_;
  CURLM* multi_handle_;
  std::atomic<bool> shutdown_;

  Thread thread_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_CORE_NETWORK_THREAD_H_
