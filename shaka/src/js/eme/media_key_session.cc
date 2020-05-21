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

#include "src/js/eme/media_key_session.h"

#include <cmath>
#include <vector>

#include "src/js/js_error.h"

namespace shaka {
namespace js {
namespace eme {

MediaKeySession::MediaKeySession(MediaKeySessionType type,
                                 std::shared_ptr<ImplementationFactory> factory,
                                 std::shared_ptr<Implementation> implementation)
    : closed(Promise::PendingPromise()),
      mutex_("MediaKeySession"),
      factory_(factory),
      implementation_(implementation),
      type_(type),
      closed_promise_(closed, /* has_value */ false) {
  AddListenerField(EventType::KeyStatusesChange, &on_key_statuses_change);
  AddListenerField(EventType::Message, &on_message);
}

// \cond Doxygen_Skip
MediaKeySession::~MediaKeySession() {}
// \endcond Doxygen_Skip


void MediaKeySession::Trace(memory::HeapTracer* tracer) const {
  EventTarget::Trace(tracer);
  tracer->Trace(&closed);
}

std::string MediaKeySession::SessionId() const {
  std::unique_lock<Mutex> lock(mutex_);
  return session_id_;
}

ExceptionOr<double> MediaKeySession::GetExpiration() const {
  const std::string session_id = SessionId();
  if (session_id.empty())
    return NAN;

  int64_t expiration;
  if (!implementation_->GetExpiration(session_id, &expiration)) {
    return JsError::TypeError("Error getting the expiration");
  }
  return expiration < 0 ? NAN : expiration;
}

ExceptionOr<ReturnVal<JsValue>> MediaKeySession::GetKeyStatuses() const {
  std::vector<KeyStatusInfo> statuses;
  const std::string session_id = SessionId();
  if (!session_id.empty() &&
      !implementation_->GetKeyStatuses(session_id, &statuses)) {
    return JsError::TypeError("Error getting the key statuses");
  }

  LocalVar<JsMap> ret(CreateMap());
  for (auto& status : statuses) {
    LocalVar<JsValue> key(
        ToJsValue(ByteBuffer(status.key_id.data(), status.key_id.size())));
    LocalVar<JsValue> value(ToJsValue(status.status));
    SetMapValue(ret, key, value);
  }
  return RawToJsValue(ret);
}

Promise MediaKeySession::GenerateRequest(MediaKeyInitDataType init_data_type,
                                         ByteBuffer init_data) {
  if (!SessionId().empty()) {
    return Promise::Rejected(JsError::DOMException(
        InvalidStateError, "Session already initialized"));
  }
  if (init_data.size() == 0) {
    return Promise::Rejected(
        JsError::TypeError("Initialization data is empty"));
  }
  if (!factory_->SupportsInitDataType(init_data_type)) {
    return Promise::Rejected(JsError::DOMException(
        NotSupportedError,
        "CDM implementation doesn't support this initialization data type"));
  }

  auto cb = [this](const std::string& session_id) {
    std::unique_lock<Mutex> lock(mutex_);
    CHECK(session_id_.empty()) << "Cannot call set_session_id() twice.";
    session_id_ = session_id;
  };
  Promise ret = Promise::PendingPromise();
  implementation_->CreateSessionAndGenerateRequest(
      EmePromise(ret, /* has_value */ false), cb, type_, init_data_type,
      Data(&init_data));
  return ret;
}

Promise MediaKeySession::Load(const std::string& session_id) {
  if (!SessionId().empty()) {
    return Promise::Rejected(JsError::DOMException(
        InvalidStateError, "Session already initialized"));
  }
  if (session_id.empty()) {
    return Promise::Rejected(JsError::TypeError("Empty session ID"));
  }
  if (type_ != MediaKeySessionType::PersistentLicense) {
    return Promise::Rejected(JsError::TypeError(
        "Cannot load a persistent license in a temporary session"));
  }

  Promise ret = Promise::PendingPromise();
  implementation_->Load(session_id, EmePromise(ret, /* has_value */ true));
  // TODO: This shouldn't be changed if the Promise is rejected.
  session_id_ = session_id;
  return ret;
}

Promise MediaKeySession::Update(ByteBuffer response) {
  const std::string session_id = SessionId();
  if (session_id.empty()) {
    return Promise::Rejected(
        JsError::DOMException(InvalidStateError, "Session not initialized"));
  }
  if (response.size() == 0) {
    return Promise::Rejected(JsError::TypeError("Empty response data"));
  }

  Promise ret = Promise::PendingPromise();
  implementation_->Update(session_id, EmePromise(ret, /* has_value */ false),
                          Data(&response));
  return ret;
}

Promise MediaKeySession::Close() {
  const std::string session_id = SessionId();
  if (session_id.empty()) {
    return Promise::Resolved();
  }

  implementation_->Close(session_id, closed_promise_);
  return closed;
}

Promise MediaKeySession::Remove() {
  const std::string session_id = SessionId();
  if (session_id.empty()) {
    return Promise::Rejected(
        JsError::DOMException(InvalidStateError, "Session not initialized"));
  }

  Promise ret = Promise::PendingPromise();
  implementation_->Remove(session_id, EmePromise(ret, /* has_value */ false));
  return ret;
}


MediaKeySessionFactory::MediaKeySessionFactory() {
  AddListenerField(EventType::KeyStatusesChange,
                   &MediaKeySession::on_key_statuses_change);
  AddListenerField(EventType::Message, &MediaKeySession::on_message);

  AddGenericProperty("sessionId", &MediaKeySession::SessionId);
  AddReadOnlyProperty("closed", &MediaKeySession::closed);

  AddGenericProperty("expiration", &MediaKeySession::GetExpiration);
  AddGenericProperty("keyStatuses", &MediaKeySession::GetKeyStatuses);

  AddMemberFunction("generateRequest", &MediaKeySession::GenerateRequest);
  AddMemberFunction("load", &MediaKeySession::Load);
  AddMemberFunction("update", &MediaKeySession::Update);
  AddMemberFunction("close", &MediaKeySession::Close);
  AddMemberFunction("remove", &MediaKeySession::Remove);
}

}  // namespace eme
}  // namespace js
}  // namespace shaka
