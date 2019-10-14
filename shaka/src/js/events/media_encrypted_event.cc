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

#include "src/js/events/media_encrypted_event.h"

namespace shaka {
namespace js {
namespace events {

DEFINE_STRUCT_SPECIAL_METHODS_MOVE_ONLY(MediaEncryptedEventInit);

MediaEncryptedEvent::MediaEncryptedEvent(
    EventType event_type, eme::MediaKeyInitDataType init_data_type,
    ByteBuffer init_data)
    : MediaEncryptedEvent(to_string(event_type), init_data_type,
                          std::move(init_data)) {}

// \cond Doxygen_Skip
MediaEncryptedEvent::~MediaEncryptedEvent() {}
// \endcond Doxygen_Skip


void MediaEncryptedEvent::Trace(memory::HeapTracer* tracer) const {
  Event::Trace(tracer);
  tracer->Trace(&init_data);
}

MediaEncryptedEvent::MediaEncryptedEvent(
    const std::string& event_type, eme::MediaKeyInitDataType init_data_type,
    ByteBuffer init_data)
    : Event(event_type),
      init_data_type(init_data_type),
      init_data(std::move(init_data)) {}

MediaEncryptedEventFactory::MediaEncryptedEventFactory() {
  AddReadOnlyProperty("initDataType", &MediaEncryptedEvent::init_data_type);
  AddReadOnlyProperty("initData", &MediaEncryptedEvent::init_data);
}

}  // namespace events
}  // namespace js
}  // namespace shaka
