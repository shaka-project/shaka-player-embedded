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

#include "src/js/eme/implementation_helper_impl.h"

#include <vector>

#include "src/core/js_manager_impl.h"
#include "src/js/eme/media_key_session.h"
#include "src/js/eme/media_keys.h"
#include "src/js/events/event.h"
#include "src/js/events/media_key_message_event.h"
#include "src/js/js_error.h"
#include "src/util/crypto.h"
#include "src/util/file_system.h"
#include "src/util/utils.h"

namespace shaka {
namespace js {
namespace eme {

ImplementationHelperImpl::ImplementationHelperImpl(
    const std::string& key_system, MediaKeys* media_keys)
    : mutex_("ImplementationHelper"),
      key_system_(key_system),
      media_keys_(media_keys) {}

ImplementationHelperImpl::~ImplementationHelperImpl() {}


std::string ImplementationHelperImpl::DataPathPrefix() const {
  std::unique_lock<Mutex> lock(mutex_);
  const std::vector<uint8_t> hash = util::HashData(
      reinterpret_cast<const uint8_t*>(key_system_.data()), key_system_.size());
  const std::string dir = util::ToHexString(hash.data(), hash.size());
  return util::FileSystem::PathJoin(
      JsManagerImpl::Instance()->GetPathForDynamicFile("eme"), dir);
}

void ImplementationHelperImpl::OnMessage(const std::string& session_id,
                                         MediaKeyMessageType message_type,
                                         const uint8_t* data,
                                         size_t data_size) const {
  std::unique_lock<Mutex> lock(mutex_);
  RefPtr<MediaKeySession> session = media_keys_->GetSession(session_id);
  if (session) {
    session->ScheduleEvent<events::MediaKeyMessageEvent>(
        EventType::Message, message_type, ByteBuffer(data, data_size));
  }
}

void ImplementationHelperImpl::OnKeyStatusChange(
    const std::string& session_id) const {
  std::unique_lock<Mutex> lock(mutex_);
  RefPtr<MediaKeySession> session = media_keys_->GetSession(session_id);
  if (session) {
    session->ScheduleEvent<events::Event>(EventType::KeyStatusesChange);
  }
}

}  // namespace eme
}  // namespace js
}  // namespace shaka
