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

#include "src/js/events/media_key_message_event.h"

namespace shaka {
namespace js {
namespace events {

DEFINE_STRUCT_SPECIAL_METHODS_MOVE_ONLY(MediaKeyMessageEventInit);

MediaKeyMessageEvent::MediaKeyMessageEvent(
    EventType event_type, eme::MediaKeyMessageType message_type,
    ByteBuffer message)
    : MediaKeyMessageEvent(to_string(event_type), message_type,
                           std::move(message)) {}

MediaKeyMessageEvent::MediaKeyMessageEvent(
    const std::string& event_type, eme::MediaKeyMessageType message_type,
    ByteBuffer message)
    : Event(event_type),
      message_type(message_type),
      message(std::move(message)) {}

// \cond Doxygen_Skip
MediaKeyMessageEvent::~MediaKeyMessageEvent() {}
// \endcond Doxygen_Skip

void MediaKeyMessageEvent::Trace(memory::HeapTracer* tracer) const {
  Event::Trace(tracer);
  tracer->Trace(&message);
}


MediaKeyMessageEventFactory::MediaKeyMessageEventFactory() {
  AddReadOnlyProperty("message", &MediaKeyMessageEvent::message);
  AddReadOnlyProperty("messageType", &MediaKeyMessageEvent::message_type);
}

}  // namespace events
}  // namespace js
}  // namespace shaka
