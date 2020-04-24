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

#include "src/js/xml_http_request.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "src/core/environment.h"
#include "src/core/js_manager_impl.h"
#include "src/js/events/event.h"
#include "src/js/events/event_names.h"
#include "src/js/events/progress_event.h"
#include "src/js/js_error.h"
#include "src/js/navigator.h"
#include "src/js/timeouts.h"
#include "src/memory/heap_tracer.h"
#include "src/util/clock.h"
#include "src/util/utils.h"

namespace shaka {
namespace js {

namespace {

/** The minimum delay, in milliseconds, between "progress" events. */
constexpr size_t kProgressInterval = 15;

constexpr const char* kCookieFileName = "net_cookies.dat";

size_t UploadCallback(void* buffer, size_t member_size, size_t member_count,
                      void* user_data) {
  auto* request = reinterpret_cast<XMLHttpRequest*>(user_data);
  auto* buffer_bytes = reinterpret_cast<uint8_t*>(buffer);
  size_t total_size = member_size * member_count;
  return request->OnUpload(buffer_bytes, total_size);
}

size_t DownloadCallback(void* buffer, size_t member_size, size_t member_count,
                        void* user_data) {
  auto* request = reinterpret_cast<XMLHttpRequest*>(user_data);
  auto* buffer_bytes = reinterpret_cast<uint8_t*>(buffer);
  size_t total_size = member_size * member_count;
  request->OnDataReceived(buffer_bytes, total_size);
  return total_size;
}

size_t HeaderCallback(char* buffer, size_t member_size, size_t member_count,
                      void* user_data) {
  auto* request = reinterpret_cast<XMLHttpRequest*>(user_data);
  auto* buffer_bytes = reinterpret_cast<uint8_t*>(buffer);
  size_t total_size = member_size * member_count;
  request->OnHeaderReceived(buffer_bytes, total_size);
  return total_size;
}

double CurrentDownloadSize(CURL* curl) {
  double ret;
  CHECK_EQ(CURLE_OK, curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &ret));
  return ret;
}

/**
 * Parses the status line of the request.  This will extract the status code
 * and message.
 * @param buffer The input buffer containing the status line.
 * @param length The length of the buffer.
 * @param code [OUT] Where to put the status code.
 * @param message [OUT] Where to put the pointer to the start of the message.
 *   It will be a pointer within |buffer|.  Namely
 *   |buffer| <= |*message| < |buffer + length|.
 * @param message_length [OUT] Where to put the length of the message.
 * @return True on success, false on error.
 */
bool ParseStatusLine(const char* buffer, size_t length, int* code,
                     const char** message, size_t* message_length) {
  // NOTE: curl will convert the status line of http2 to match http1.1.
  // e.g.: "HTTP/1.1 200 OK\r\n"
  constexpr const char kMinStatusLine[] = "HTTP/1.1 200 \r\n";
  constexpr const char kHttp10[] = "HTTP/1.0 ";
  constexpr const char kHttp11[] = "HTTP/1.1 ";
  constexpr const size_t kMinStatusSize = sizeof(kMinStatusLine) - 1;
  constexpr const size_t kHttp10Size = sizeof(kHttp10) - 1;
  constexpr const size_t kHttp11Size = sizeof(kHttp11) - 1;
  constexpr const size_t kStatusSize = 3;

  if (length < kMinStatusSize)
    return false;
  if (strncmp(buffer, kHttp10, kHttp10Size) != 0 &&
      strncmp(buffer, kHttp11, kHttp11Size) != 0)
    return false;

  // Convert the status code, must be three numbers [0-9].
  const char* status_code = buffer + kHttp10Size;
  const char* status_code_end = status_code + kStatusSize;
  if (!std::isdigit(status_code[0]) || !std::isdigit(status_code[1]) ||
      !std::isdigit(status_code[2])) {
    return false;
  }
  if (status_code_end[0] != ' ')
    return false;
  if (buffer[length - 2] != '\r' || buffer[length - 1] != '\n')
    return false;

  *code = std::stoi(std::string(status_code, status_code_end));
  *message = status_code_end + 1;
  *message_length = length - kMinStatusSize;
  return true;
}

}  // namespace

XMLHttpRequest::XMLHttpRequest()
    : ready_state(XMLHttpRequest::ReadyState::Unsent),
      mutex_("XMLHttpRequest"),
      curl_(curl_easy_init()),
      request_headers_(nullptr),
      with_credentials_(false) {
  AddListenerField(EventType::Abort, &on_abort);
  AddListenerField(EventType::Error, &on_error);
  AddListenerField(EventType::Load, &on_load);
  AddListenerField(EventType::LoadStart, &on_load_start);
  AddListenerField(EventType::Progress, &on_progress);
  AddListenerField(EventType::ReadyStateChange, &on_ready_state_change);
  AddListenerField(EventType::Timeout, &on_timeout);
  AddListenerField(EventType::LoadEnd, &on_load_end);

  Reset();
}

// \cond Doxygen_Skip
XMLHttpRequest::~XMLHttpRequest() {
  // Don't call Abort() since we can't raise events.
  abort_pending_ = true;
  JsManagerImpl::Instance()->NetworkThread()->AbortRequest(this);

  curl_easy_cleanup(curl_);
  if (request_headers_)
    curl_slist_free_all(request_headers_);
  request_headers_ = nullptr;
}
// \endcond Doxygen_Skip

void XMLHttpRequest::Trace(memory::HeapTracer* tracer) const {
  // No need to trace on_* members as EventTarget handles it.
  EventTarget::Trace(tracer);
  std::unique_lock<Mutex> lock(mutex_);
  tracer->Trace(&response);
  tracer->Trace(&upload_data_);
}

bool XMLHttpRequest::IsShortLived() const {
  return true;
}

void XMLHttpRequest::Abort() {
  if (!JsManagerImpl::Instance()->NetworkThread()->ContainsRequest(this))
    return;

  abort_pending_ = true;
  JsManagerImpl::Instance()->NetworkThread()->AbortRequest(this);

  std::unique_lock<Mutex> lock(mutex_);
  if (ready_state != XMLHttpRequest::ReadyState::Done) {
    // Fire events synchronously.
    ready_state = XMLHttpRequest::ReadyState::Done;
    RaiseEvent<events::Event>(EventType::ReadyStateChange);

    double total_size = CurrentDownloadSize(curl_);
    RaiseEvent<events::ProgressEvent>(EventType::Progress, true, total_size,
                                      total_size);
    RaiseEvent<events::Event>(EventType::Abort);
    RaiseEvent<events::ProgressEvent>(EventType::LoadEnd, true, total_size,
                                      total_size);
  }

  // The spec says at the end to change the ready state without firing an event.
  // https://xhr.spec.whatwg.org/#the-abort()-method
  ready_state = XMLHttpRequest::ReadyState::Unsent;
}

std::string XMLHttpRequest::GetAllResponseHeaders() const {
  std::unique_lock<Mutex> lock(mutex_);
  std::string ret;
  for (auto it : response_headers_) {
    ret.append(it.first + ": " + it.second + "\r\n");
  }
  return ret;
}

shaka::optional<std::string> XMLHttpRequest::GetResponseHeader(
    const std::string& name) const {
  std::unique_lock<Mutex> lock(mutex_);
  auto it = response_headers_.find(name);
  if (it == response_headers_.end())
    return nullopt;
  return it->second;
}

ExceptionOr<void> XMLHttpRequest::Open(const std::string& method,
                                       const std::string& url,
                                       optional<bool> async,
                                       optional<std::string> user,
                                       optional<std::string> password) {
  if (async.has_value() && !async.value()) {
    return JsError::DOMException(NotSupportedError,
                                 "Synchronous requests are not supported.");
  }

  // This will call Abort() which may call back into JavaScript by firing
  // events synchronously.
  Reset();

  std::unique_lock<Mutex> lock(mutex_);
  this->ready_state = XMLHttpRequest::ReadyState::Opened;
  ScheduleEvent<events::Event>(EventType::ReadyStateChange);

  curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, method.c_str());
  if (method == "HEAD")
    curl_easy_setopt(curl_, CURLOPT_NOBODY, 1L);

  if (user.has_value())
    curl_easy_setopt(curl_, CURLOPT_USERNAME, user->c_str());
  if (password.has_value())
    curl_easy_setopt(curl_, CURLOPT_PASSWORD, password->c_str());

  return {};
}

ExceptionOr<void> XMLHttpRequest::Send(
    optional<variant<ByteBuffer, ByteString>> maybe_data) {
  // Don't query while locked to avoid a deadlock.
  const bool contains_request =
      JsManagerImpl::Instance()->NetworkThread()->ContainsRequest(this);

  {
    std::unique_lock<Mutex> lock(mutex_);
    // If we are not open, or if the request has already been sent.
    if (ready_state != XMLHttpRequest::ReadyState::Opened || contains_request) {
      return JsError::DOMException(InvalidStateError,
                                   "The object's state must be OPENED.");
    }
    if (response_type != "arraybuffer") {
      return JsError::DOMException(
          NotSupportedError,
          "Response type " + response_type + " is not supported");
    }

    if (maybe_data.has_value()) {
      if (holds_alternative<ByteBuffer>(*maybe_data)) {
        upload_data_ = std::move(get<ByteBuffer>(*maybe_data));
      } else {
        ByteString* str = &get<ByteString>(*maybe_data);
        upload_data_.SetFromBuffer(str->data(), str->size());
      }

      upload_pos_ = 0;
      curl_easy_setopt(curl_, CURLOPT_UPLOAD, 1L);
      curl_easy_setopt(curl_, CURLOPT_INFILESIZE_LARGE,
                       static_cast<curl_off_t>(upload_data_.size()));
      curl_easy_setopt(curl_, CURLOPT_READDATA, this);
      curl_easy_setopt(curl_, CURLOPT_READFUNCTION, UploadCallback);
    } else {
      curl_easy_setopt(curl_, CURLOPT_UPLOAD, 0L);
    }

    curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS,
                     static_cast<long>(timeout_ms));  // NOLINT
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, request_headers_);
  }

  // Don't add while locked to avoid a deadlock.
  JsManagerImpl::Instance()->NetworkThread()->AddRequest(this);
  return {};
}

ExceptionOr<void> XMLHttpRequest::SetRequestHeader(const std::string& key,
                                                   const std::string& value) {
  std::unique_lock<Mutex> lock(mutex_);
  if (ready_state != XMLHttpRequest::ReadyState::Opened) {
    return JsError::DOMException(InvalidStateError,
                                 "The object's state must be OPENED.");
  }
  const std::string header = key + ": " + value;
  request_headers_ = curl_slist_append(request_headers_, header.c_str());
  return {};
}

bool XMLHttpRequest::WithCredentials() const {
  return with_credentials_;
}

ExceptionOr<void> XMLHttpRequest::SetWithCredentials(bool with_credentials) {
  if (ready_state != XMLHttpRequest::ReadyState::Unsent &&
      ready_state != XMLHttpRequest::ReadyState::Opened) {
    return JsError::DOMException(
        InvalidStateError,
        "withCredentials can only be set if the object's state is UNSENT or "
        "OPENED.");
  }
  with_credentials_ = with_credentials;
  return {};
}

void XMLHttpRequest::RaiseProgressEvents() {
  if (abort_pending_)
    return;

  if (ready_state == XMLHttpRequest::ReadyState::Opened) {
    this->ready_state = XMLHttpRequest::ReadyState::HeadersReceived;
    RaiseEvent<events::Event>(EventType::ReadyStateChange);
  }
  if (ready_state != XMLHttpRequest::ReadyState::Loading) {
    this->ready_state = XMLHttpRequest::ReadyState::Loading;
    RaiseEvent<events::Event>(EventType::ReadyStateChange);
  }

  double cur_size;
  {
    std::unique_lock<Mutex> lock(mutex_);
    cur_size = CurrentDownloadSize(curl_);
  }
  RaiseEvent<events::ProgressEvent>(EventType::Progress, estimated_size_ != 0,
                                    cur_size, estimated_size_);
}

void XMLHttpRequest::OnDataReceived(uint8_t* buffer, size_t length) {
  std::unique_lock<Mutex> lock(mutex_);

  // We need to schedule these events from this callback since we don't know
  // when the last header will be received.
  const uint64_t now = util::Clock::Instance.GetMonotonicTime();
  if (!abort_pending_ && now - last_progress_time_ >= kProgressInterval) {
    last_progress_time_ = now;
    JsManagerImpl::Instance()->MainThread()->AddInternalTask(
        TaskPriority::Internal, "Schedule XHR events",
        MemberCallbackTask(this, &XMLHttpRequest::RaiseProgressEvents));
  }

  temp_data_.AppendCopy(buffer, length);
}

void XMLHttpRequest::OnHeaderReceived(const uint8_t* buffer, size_t length) {
  std::unique_lock<Mutex> lock(mutex_);
  // This method is called for each header (including status line) for the
  // duration of the request, this includes redirects.
  // https://curl.haxx.se/libcurl/c/CURLOPT_HEADERFUNCTION.html
  //
  // Be careful about using string methods.  This data may not be null-
  // terminated.  Be sure to use the |length| parameter and always pass the
  // explicit end to std::string.
  auto* str = reinterpret_cast<const char*>(buffer);
  if (!parsing_headers_) {
    const char* message;
    size_t message_size;
    if (!ParseStatusLine(str, length, &status, &message, &message_size))
      return;
    status_text.assign(message, message + message_size);

    parsing_headers_ = true;
    // Clear headers from the previous request.  This is important for
    // redirects so we don't keep headers from the redirect.
    response_headers_.clear();
  } else {
    // 'Content-Length: 123\r\n'
    const char* sep = std::find(str, str + length, ':');
    // |sep == str + length| if not found.
    const size_t key_len = sep - str;
    const size_t rest_len = length - key_len;  // == 0 if not found.
    if (rest_len >= 2 && sep[rest_len - 2] == '\r' &&
        sep[rest_len - 1] == '\n') {
      std::string key = util::ToAsciiLower(std::string(str, sep));
      std::string value =
          util::TrimAsciiWhitespace(std::string(sep + 1, sep + rest_len - 1));

      if (response_headers_.count(key) == 0)
        response_headers_[key] = value;
      else
        response_headers_[key] += ", " + value;

      // Parse content-length so we can get the size of the download.
      if (key == "content-length") {
        errno = 0;  // |errno| is thread_local.
        char* end;
        const auto size = strtol(value.c_str(), &end, 10);
        if (errno != ERANGE && end == value.c_str() + value.size()) {
          estimated_size_ = size;
        }
      }
    }
    // Ignore invalid headers.
  }
  if (length == 2 && str[0] == '\r' && str[1] == '\n') {
    // An empty header signals the end of the headers for the current request.
    // If there is a redirect or the XMLHttpRequest object is used to make
    // another request then we should parse the status line.
    parsing_headers_ = false;
  }
}

size_t XMLHttpRequest::OnUpload(uint8_t* buffer, size_t length) {
  std::unique_lock<Mutex> lock(mutex_);
  const size_t remaining = upload_data_.size() - upload_pos_;
  if (length > remaining)
    length = remaining;
  std::memcpy(buffer, upload_data_.data() + upload_pos_, length);
  upload_pos_ += length;
  return length;
}

void XMLHttpRequest::Reset() {
  Abort();
  response.Clear();
  response_text = "";
  response_type = "arraybuffer";
  response_url = "";
  status = 0;
  status_text = "";
  timeout_ms = 0;

  last_progress_time_ = 0;
  estimated_size_ = 0;
  parsing_headers_ = false;
  abort_pending_ = false;

  response_headers_.clear();
  temp_data_.Clear();
  upload_data_.Clear();

  curl_easy_reset(curl_);
  curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, DownloadCallback);
  curl_easy_setopt(curl_, CURLOPT_WRITEDATA, this);
  curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, HeaderCallback);
  curl_easy_setopt(curl_, CURLOPT_HEADERDATA, this);
  curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl_, CURLOPT_USERAGENT, USER_AGENT);

  const std::string cookie_file =
      JsManagerImpl::Instance()->GetPathForDynamicFile(kCookieFileName);
  curl_easy_setopt(curl_, CURLOPT_COOKIEFILE, cookie_file.c_str());
  curl_easy_setopt(curl_, CURLOPT_COOKIEJAR, cookie_file.c_str());

  // Don't batch up TCP packets.
  curl_easy_setopt(curl_, CURLOPT_TCP_NODELAY, 1L);
  // Don't wait for a 100 Continue for uploads.
  curl_easy_setopt(curl_, CURLOPT_EXPECT_100_TIMEOUT_MS, 1L);

  if (request_headers_)
    curl_slist_free_all(request_headers_);
  request_headers_ = nullptr;
}

void XMLHttpRequest::OnRequestComplete(CURLcode code) {
  // Careful, this is called from the worker thread, so we cannot call into V8.
  std::unique_lock<Mutex> lock(mutex_);
  if (code == CURLE_OK) {
    response_text = temp_data_.CreateString();
    response.SetFromDynamicBuffer(temp_data_);
    temp_data_.Clear();

    char* url;
    curl_easy_getinfo(curl_, CURLINFO_EFFECTIVE_URL, &url);
    response_url = url;

    // Flush cookie list to disk so other instances can access them.
    curl_easy_setopt(curl_, CURLOPT_COOKIELIST, "FLUSH");
  } else {
    // Don't need to reset everything on error because it was reset in Send().
    // But we do need to set these as they are set in OnHeaderReceived.
    status = 0;
    status_text = "";
  }

  // Don't schedule events if we are aborted, they will be called within
  // Abort().
  if (!abort_pending_) {
    this->ready_state = XMLHttpRequest::ReadyState::Done;
    ScheduleEvent<events::Event>(EventType::ReadyStateChange);

    double total_size = CurrentDownloadSize(curl_);
    ScheduleEvent<events::ProgressEvent>(EventType::Progress, true, total_size,
                                         total_size);
    switch (code) {
      case CURLE_OK:
        ScheduleEvent<events::Event>(EventType::Load);
        break;
      case CURLE_OPERATION_TIMEDOUT:
        ScheduleEvent<events::Event>(EventType::Timeout);
        break;
      default:
        LOG(ERROR) << "Error returned by curl: " << code;
        ScheduleEvent<events::Event>(EventType::Error);
        break;
    }
    ScheduleEvent<events::ProgressEvent>(EventType::LoadEnd, true, total_size,
                                         total_size);
  }
}


XMLHttpRequestFactory::XMLHttpRequestFactory() {
  AddConstant("UNSENT", XMLHttpRequest::ReadyState::Unsent);
  AddConstant("OPENED", XMLHttpRequest::ReadyState::Opened);
  AddConstant("HEADERS_RECEIVED", XMLHttpRequest::ReadyState::HeadersReceived);
  AddConstant("LOADING", XMLHttpRequest::ReadyState::Loading);
  AddConstant("DONE", XMLHttpRequest::ReadyState::Done);

  AddReadOnlyProperty("readyState", &XMLHttpRequest::ready_state);
  AddReadOnlyProperty("response", &XMLHttpRequest::response);
  AddReadOnlyProperty("responseText", &XMLHttpRequest::response_text);
  AddReadWriteProperty("responseType", &XMLHttpRequest::response_type);
  AddReadOnlyProperty("responseURL", &XMLHttpRequest::response_url);
  AddReadOnlyProperty("status", &XMLHttpRequest::status);
  AddReadOnlyProperty("statusText", &XMLHttpRequest::status_text);
  AddReadWriteProperty("timeout", &XMLHttpRequest::timeout_ms);
  AddGenericProperty("withCredentials", &XMLHttpRequest::WithCredentials,
                     &XMLHttpRequest::SetWithCredentials);

  AddListenerField(EventType::Abort, &XMLHttpRequest::on_abort);
  AddListenerField(EventType::Error, &XMLHttpRequest::on_error);
  AddListenerField(EventType::Load, &XMLHttpRequest::on_load);
  AddListenerField(EventType::LoadStart, &XMLHttpRequest::on_load_start);
  AddListenerField(EventType::Progress, &XMLHttpRequest::on_progress);
  AddListenerField(EventType::ReadyStateChange,
                   &XMLHttpRequest::on_ready_state_change);
  AddListenerField(EventType::Timeout, &XMLHttpRequest::on_timeout);
  AddListenerField(EventType::LoadEnd, &XMLHttpRequest::on_load_end);

  AddMemberFunction("abort", &XMLHttpRequest::Abort);
  AddMemberFunction("getAllResponseHeaders",
                    &XMLHttpRequest::GetAllResponseHeaders);
  AddMemberFunction("getResponseHeader", &XMLHttpRequest::GetResponseHeader);
  AddMemberFunction("open", &XMLHttpRequest::Open);
  AddMemberFunction("send", &XMLHttpRequest::Send);
  AddMemberFunction("setRequestHeader", &XMLHttpRequest::SetRequestHeader);
}

}  // namespace js
}  // namespace shaka
