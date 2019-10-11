// Copyright 2019 Google LLC
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

#ifndef SHAKA_EMBEDDED_JS_EVENTS_VERSION_CHANGE_EVENT_H_
#define SHAKA_EMBEDDED_JS_EVENTS_VERSION_CHANGE_EVENT_H_

#include <memory>
#include <string>

#include "shaka/optional.h"
#include "src/js/events/event.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/struct.h"

namespace shaka {
namespace js {
namespace events {

struct IDBVersionChangeEventInit : Struct {
  DECLARE_STRUCT_SPECIAL_METHODS(IDBVersionChangeEventInit);

  ADD_DICT_FIELD(oldVersion, uint64_t);
  ADD_DICT_FIELD(newVersion, optional<uint64_t>);
};

/**
 * See: https://w3c.github.io/IndexedDB/#idbversionchangeevent
 */
class IDBVersionChangeEvent final : public Event {
  DECLARE_TYPE_INFO(IDBVersionChangeEvent);

 public:
  IDBVersionChangeEvent(EventType event_type, uint64_t old_version,
                        optional<uint64_t> new_version);

  static IDBVersionChangeEvent* Create(
      const std::string& event_type,
      optional<IDBVersionChangeEventInit> init_data) {
    if (init_data.has_value()) {
      return new IDBVersionChangeEvent(event_type, init_data->oldVersion,
                                       init_data->newVersion.value());
    } else {
      return new IDBVersionChangeEvent(event_type, 0, nullopt);
    }
  }

  const uint64_t old_version;
  const optional<uint64_t> new_version;

 private:
  IDBVersionChangeEvent(const std::string& event_type, uint64_t old_version,
                        optional<uint64_t> new_version);
};

class IDBVersionChangeEventFactory final
    : public BackingObjectFactory<IDBVersionChangeEvent, Event> {
 public:
  IDBVersionChangeEventFactory();
};

}  // namespace events
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_EVENTS_VERSION_CHANGE_EVENT_H_
