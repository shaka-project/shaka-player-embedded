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

#include "src/core/network_thread.h"

#include <curl/curl.h>
#include <sys/select.h>

#include <algorithm>
#include <cerrno>

#include "src/js/xml_http_request.h"
#include "src/util/utils.h"

namespace shaka {

namespace {

constexpr const long kSmallDelayMs = 100;  // NOLINT
constexpr const long kMaxDelayMs = 500;    // NOLINT

}  // namespace

NetworkThread::NetworkThread()
    : mutex_("NetworkThread"),
      cond_("Networking new request"),
      multi_handle_(curl_multi_init()),
      shutdown_(false),
      thread_("Networking", std::bind(&NetworkThread::ThreadMain, this)) {
  CHECK(multi_handle_);
}

NetworkThread::~NetworkThread() {
  CHECK(!thread_.joinable()) << "Need to call Stop() before destroying";
  DCHECK(requests_.empty());
  curl_multi_cleanup(multi_handle_);
}

void NetworkThread::Stop() {
  shutdown_.store(true, std::memory_order_release);
  cond_.SignalAllIfNotSet();
  thread_.join();
}

bool NetworkThread::ContainsRequest(RefPtr<js::XMLHttpRequest> request) const {
  std::unique_lock<Mutex> lock(mutex_);
  return util::contains(requests_, request);
}

void NetworkThread::AddRequest(RefPtr<js::XMLHttpRequest> request) {
  std::unique_lock<Mutex> lock(mutex_);
  DCHECK(!shutdown_.load(std::memory_order_acquire));
  DCHECK(!util::contains(requests_, request));
  requests_.push_back(request);
  CHECK_EQ(curl_multi_add_handle(multi_handle_, request->curl_), CURLM_OK);
  cond_.SignalAllIfNotSet();
}

void NetworkThread::AbortRequest(RefPtr<js::XMLHttpRequest> request) {
  std::unique_lock<Mutex> lock(mutex_);
  for (auto it = requests_.begin(); it != requests_.end(); it++) {
    if (*it == request) {
      CHECK_EQ(curl_multi_remove_handle(multi_handle_, request->curl_),
               CURLM_OK);
      requests_.erase(it);
      break;
    }
  }
}

void NetworkThread::ThreadMain() {
  while (!shutdown_.load(std::memory_order_acquire)) {
    fd_set fdread;
    fd_set fdwrite;
    fd_set fdexc;
    long timeout_ms = -1;  // NOLINT
    int maxfd = -1;
    bool no_handles;
    {
      std::unique_lock<Mutex> lock(mutex_);
      // This will still return success if there are no requests or if there is
      // an error in one request.
      int handles = 0;
      CHECK_EQ(curl_multi_perform(multi_handle_, &handles), CURLM_OK);
      no_handles = handles == 0;

      // Get any pending messages and complete any requests that are done.
      int msg_count;
      while (CURLMsg* msg = curl_multi_info_read(multi_handle_, &msg_count)) {
        if (msg->msg == CURLMSG_DONE) {
          for (auto it = requests_.begin(); it != requests_.end(); it++) {
            if ((*it)->curl_ == msg->easy_handle) {
              (*it)->OnRequestComplete(msg->data.result);  // NOLINT
              requests_.erase(it);
              break;
            }
          }
          CHECK_EQ(curl_multi_remove_handle(multi_handle_, msg->easy_handle),
                   CURLM_OK);
        } else {
          // There are currently no other message types.
          LOG(DFATAL) << "Unknown message type: " << msg->msg;
        }
      }

      if (curl_multi_fdset(multi_handle_, &fdread, &fdwrite, &fdexc, &maxfd) !=
          CURLM_OK) {
        LOG(ERROR) << "Error getting file descriptors from CURL";
      }
      if (curl_multi_timeout(multi_handle_, &timeout_ms) != CURLM_OK) {
        LOG(ERROR) << "Error getting timeout from CURL";
      }
      if (timeout_ms < 0)
        // If we failed to query CURL, use a default.
        timeout_ms = kSmallDelayMs;
      else
        timeout_ms = std::min(timeout_ms, kMaxDelayMs);
    }

    // Wait until we have something to do.
    if (no_handles) {
      std::unique_lock<Mutex> lock(mutex_);
      cond_.ResetAndWaitWhileUnlocked(lock);
    } else {
      timeval timeout = {.tv_sec = timeout_ms / 1000,
                         .tv_usec = (timeout_ms % 1000) * 1000};
      if (select(maxfd + 1, &fdread, &fdwrite, &fdexc, &timeout) < 0) {
        if (errno == EBADF) {
          // If another thread aborts the request, it will close the file
          // descriptor, causing an error here, so just ignore it.
        } else {
          PLOG(ERROR) << "Error waiting for network handles";
        }
      }
    }
  }
}

}  // namespace shaka
