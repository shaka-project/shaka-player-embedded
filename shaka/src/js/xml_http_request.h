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

#ifndef SHAKA_EMBEDDED_JS_XML_HTTP_REQUEST_H_
#define SHAKA_EMBEDDED_JS_XML_HTTP_REQUEST_H_

#include <curl/curl.h>

#include <atomic>
#include <map>
#include <string>

#include "shaka/optional.h"
#include "shaka/variant.h"
#include "src/debug/mutex.h"
#include "src/js/events/event_target.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/byte_buffer.h"
#include "src/mapping/byte_string.h"
#include "src/mapping/enum.h"
#include "src/mapping/exception_or.h"
#include "src/util/dynamic_buffer.h"

namespace shaka {
class NetworkThread;

namespace js {

/**
 * An implementation of JavaScript XMLHttpRequest.  This handles network
 * requests using CURL.
 *
 * Notes:
 * - Only supports asynchronous mode.
 * - Only support 'arraybuffer' responseType, but still sets responseText.
 * - Send() supports string, ArrayBuffer, or ArrayBufferView.
 * - Supports responseURL.
 * - Supports request/response headers.
 * - Support Abort().
 * - Fires abort, readystatechange, progress, load, timeout, and loadend events.
 *
 * IMPORTANT:
 * - Ignores CORS.
 * - Ignores withCredentials.
 * - Does not validate request headers.
 */
class XMLHttpRequest : public events::EventTarget {
  DECLARE_TYPE_INFO(XMLHttpRequest);

 public:
  enum class ReadyState {
    Unsent = 0,
    Opened = 1,
    HeadersReceived = 2,
    Loading = 3,
    Done = 4,
  };

  XMLHttpRequest();
  static XMLHttpRequest* Create() {
    return new XMLHttpRequest();
  }

  void Trace(memory::HeapTracer* tracer) const override;
  bool IsShortLived() const override;

  void Abort();
  std::string GetAllResponseHeaders() const;
  optional<std::string> GetResponseHeader(const std::string& name) const;
  ExceptionOr<void> Open(const std::string& method, const std::string& url,
                         optional<bool> async, optional<std::string> user,
                         optional<std::string> password);
  ExceptionOr<void> Send(optional<variant<ByteBuffer, ByteString>> maybe_data);
  ExceptionOr<void> SetRequestHeader(const std::string& key,
                                     const std::string& value);

  /**
   * Called from a CURL callback when (part of) the body data is received.
   */
  void OnDataReceived(uint8_t* buffer, size_t length);

  /**
   * Called from a CURL callback for each header that is received.
   * @param buffer A buffer containing exactly one header.
   * @param length The length of |buffer|.
   */
  void OnHeaderReceived(const uint8_t* buffer, size_t length);

  /** Called from a CURL callback when uploading data. */
  size_t OnUpload(uint8_t* buffer, size_t length);

  Listener on_abort;
  Listener on_error;
  Listener on_load;
  Listener on_load_start;
  Listener on_progress;
  Listener on_ready_state_change;
  Listener on_timeout;
  Listener on_load_end;

  ReadyState ready_state;
  ByteBuffer response;
  std::string response_text;
  std::string response_type;
  std::string response_url;
  int status;
  std::string status_text;
  uint64_t timeout_ms;  // JavaScript "timeout"
  bool with_credentials;

 private:
  friend NetworkThread;

  void RaiseProgressEvents();

  /** Called when the request completes. */
  void OnRequestComplete(CURLcode code);

  void Reset();

  mutable Mutex mutex_;
  std::map<std::string, std::string> response_headers_;
  util::DynamicBuffer temp_data_;
  ByteBuffer upload_data_;

  CURL* curl_;
  curl_slist* request_headers_;
  size_t upload_pos_;
  uint64_t last_progress_time_;
  double estimated_size_;
  bool parsing_headers_;
  std::atomic<bool> abort_pending_;
};

class XMLHttpRequestFactory
    : public BackingObjectFactory<XMLHttpRequest, events::EventTarget> {
 public:
  XMLHttpRequestFactory();
};

}  // namespace js
}  // namespace shaka

CONVERT_ENUM_AS_NUMBER(shaka::js, XMLHttpRequest::ReadyState);

#endif  // SHAKA_EMBEDDED_JS_XML_HTTP_REQUEST_H_
