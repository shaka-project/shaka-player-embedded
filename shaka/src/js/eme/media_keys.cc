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

#include "src/js/eme/media_keys.h"

#include "src/js/eme/media_key_session.h"
#include "src/js/js_error.h"
#include "src/mapping/convert_js.h"

namespace shaka {
namespace js {
namespace eme {

MediaKeys::MediaKeys(ImplementationFactory* factory,
                     const std::string& key_system,
                     const MediaKeySystemConfiguration& config)
    : helper_(key_system, this), factory_(factory) {
  std::vector<std::string> audio_robustness, video_robustness;

  for (auto& item : config.audioCapabilities) {
    audio_robustness.push_back(item.robustness);
  }
  for (auto& item : config.videoCapabilities) {
    video_robustness.push_back(item.robustness);
  }

  implementation_ = factory_->CreateImplementation(
      &helper_, config.distinctiveIdentifier, config.persistentState,
      audio_robustness, video_robustness);
}

// \cond Doxygen_Skip
MediaKeys::~MediaKeys() {
  if (implementation_) {
    implementation_->Destroy();
    implementation_ = nullptr;
  }
}
// \endcond Doxygen_Skip

void MediaKeys::Trace(memory::HeapTracer* tracer) const {
  BackingObject::Trace(tracer);

  std::unique_lock<std::mutex> lock(mutex_);
  tracer->Trace(&sessions_);
}

bool MediaKeys::valid() const {
  return implementation_;
}

ExceptionOr<RefPtr<MediaKeySession>> MediaKeys::CreateSession(
    optional<MediaKeySessionType> session_type) {
  const MediaKeySessionType type =
      session_type.value_or(MediaKeySessionType::Temporary);
  if (!factory_->SupportsSessionType(type)) {
    return JsError::DOMException(NotSupportedError,
                                 "The given session type is not supported.");
  }

  std::unique_lock<std::mutex> lock(mutex_);
  RefPtr<MediaKeySession> session =
      new MediaKeySession(type, factory_, implementation_, &helper_);
  sessions_.emplace_back(session);
  return session;
}

Promise MediaKeys::SetServerCertificate(ByteBuffer cert) {
  Promise ret;
  implementation_->SetServerCertificate(EmePromise(ret, /* has_value */ true),
                                        Data(&cert));
  return ret;
}

RefPtr<MediaKeySession> MediaKeys::GetSession(const std::string& session_id) {
  std::unique_lock<std::mutex> lock(mutex_);
  for (auto& session : sessions_) {
    if (session->SessionId() == session_id)
      return session;
  }
  return nullptr;
}


MediaKeysFactory::MediaKeysFactory() {
  AddMemberFunction("createSession", &MediaKeys::CreateSession);
  AddMemberFunction("setServerCertificate", &MediaKeys::SetServerCertificate);
}

}  // namespace eme
}  // namespace js
}  // namespace shaka
