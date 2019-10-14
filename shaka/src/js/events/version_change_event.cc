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

#include "src/js/events/version_change_event.h"

namespace shaka {
namespace js {
namespace events {

DEFINE_STRUCT_SPECIAL_METHODS_COPYABLE(IDBVersionChangeEventInit);

IDBVersionChangeEvent::IDBVersionChangeEvent(EventType event_type,
                                             uint64_t old_version,
                                             optional<uint64_t> new_version)
    : IDBVersionChangeEvent(to_string(event_type), old_version, new_version) {}

// \cond Doxygen_Skip
IDBVersionChangeEvent::~IDBVersionChangeEvent() {}
// \endcond Doxygen_Skip


IDBVersionChangeEvent::IDBVersionChangeEvent(const std::string& event_type,
                                             uint64_t old_version,
                                             optional<uint64_t> new_version)
    : Event(event_type), old_version(old_version), new_version(new_version) {}

IDBVersionChangeEventFactory::IDBVersionChangeEventFactory() {
  AddReadOnlyProperty("oldVersion", &IDBVersionChangeEvent::old_version);
  AddReadOnlyProperty("newVersion", &IDBVersionChangeEvent::new_version);
}

}  // namespace events
}  // namespace js
}  // namespace shaka
