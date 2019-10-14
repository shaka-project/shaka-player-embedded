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

#ifndef SHAKA_EMBEDDED_JS_EVENTS_MEDIA_ENCRYPTED_EVENT_H_
#define SHAKA_EMBEDDED_JS_EVENTS_MEDIA_ENCRYPTED_EVENT_H_

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

struct MediaEncryptedEventInit : Struct {
  DECLARE_STRUCT_SPECIAL_METHODS_MOVE_ONLY(MediaEncryptedEventInit);

  ADD_DICT_FIELD(initDataType, eme::MediaKeyInitDataType);
  ADD_DICT_FIELD(initData, ByteBuffer);
};


/**
 * See: https://w3c.github.io/encrypted-media/#dom-mediaencryptedevent
 */
class MediaEncryptedEvent final : public Event {
  DECLARE_TYPE_INFO(MediaEncryptedEvent);

 public:
  MediaEncryptedEvent(EventType event_type,
                      eme::MediaKeyInitDataType init_data_type,
                      ByteBuffer init_data);

  static MediaEncryptedEvent* Create(
      const std::string& event_type,
      optional<MediaEncryptedEventInit> init_data) {
    if (init_data.has_value()) {
      return new MediaEncryptedEvent(event_type, init_data->initDataType,
                                     std::move(init_data->initData));
    } else {
      return new MediaEncryptedEvent(
          event_type, eme::MediaKeyInitDataType::Cenc, ByteBuffer());
    }
  }

  void Trace(memory::HeapTracer* tracer) const override;

  const eme::MediaKeyInitDataType init_data_type;
  const ByteBuffer init_data;

 private:
  MediaEncryptedEvent(const std::string& event_type,
                      eme::MediaKeyInitDataType init_data_type,
                      ByteBuffer init_data);
};

class MediaEncryptedEventFactory final
    : public BackingObjectFactory<MediaEncryptedEvent, Event> {
 public:
  MediaEncryptedEventFactory();
};

}  // namespace events
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_EVENTS_MEDIA_ENCRYPTED_EVENT_H_
