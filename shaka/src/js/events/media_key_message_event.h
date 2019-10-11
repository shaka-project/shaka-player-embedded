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

#ifndef SHAKA_EMBEDDED_JS_EVENTS_MEDIA_KEY_MESSAGE_EVENT_H_
#define SHAKA_EMBEDDED_JS_EVENTS_MEDIA_KEY_MESSAGE_EVENT_H_

#include <memory>
#include <string>
#include <utility>

#include "shaka/optional.h"
#include "src/js/eme/media_key_system_configuration.h"
#include "src/js/events/event.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/byte_buffer.h"
#include "src/mapping/struct.h"

namespace shaka {
namespace js {
namespace events {

struct MediaKeyMessageEventInit : Struct {
  static std::string name() {
    return "MediaKeyMessageEventInit";
  }

  ADD_DICT_FIELD(messageType, eme::MediaKeyMessageType);
  ADD_DICT_FIELD(message, ByteBuffer);
};


/**
 * See: https://w3c.github.io/encrypted-media/#dom-mediakeymessageevent
 */
class MediaKeyMessageEvent final : public Event {
  DECLARE_TYPE_INFO(MediaKeyMessageEvent);

 public:
  MediaKeyMessageEvent(EventType event_type,
                       eme::MediaKeyMessageType message_type,
                       ByteBuffer message);

  static MediaKeyMessageEvent* Create(
      const std::string& event_type,
      optional<MediaKeyMessageEventInit> init_data) {
    if (init_data.has_value()) {
      return new MediaKeyMessageEvent(event_type, init_data->messageType,
                                      std::move(init_data->message));
    } else {
      return new MediaKeyMessageEvent(
          event_type, eme::MediaKeyMessageType::LicenseRequest, ByteBuffer());
    }
  }

  void Trace(memory::HeapTracer* tracer) const override;

  const eme::MediaKeyMessageType message_type;
  const ByteBuffer message;

 private:
  MediaKeyMessageEvent(const std::string& event_type,
                       eme::MediaKeyMessageType message_type,
                       ByteBuffer message);
};

class MediaKeyMessageEventFactory final
    : public BackingObjectFactory<MediaKeyMessageEvent, Event> {
 public:
  MediaKeyMessageEventFactory();
};

}  // namespace events
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_EVENTS_MEDIA_KEY_MESSAGE_EVENT_H_
