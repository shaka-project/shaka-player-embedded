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

#include "src/js/events/progress_event.h"

namespace shaka {
namespace js {
namespace events {

ProgressEvent::ProgressEvent(EventType type, bool length_computable,
                             double loaded, double total)
    : ProgressEvent(to_string(type), length_computable, loaded, total) {}

ProgressEvent::ProgressEvent(const std::string& type, bool length_computable,
                             double loaded, double total)
    : Event(type),
      length_computable(length_computable),
      loaded(loaded),
      total(total) {}

// \cond Doxygen_Skip
ProgressEvent::~ProgressEvent() {}
// \endcond Doxygen_Skip


ProgressEventFactory::ProgressEventFactory() {
  AddReadOnlyProperty("lengthComputable", &ProgressEvent::length_computable);
  AddReadOnlyProperty("loaded", &ProgressEvent::loaded);
  AddReadOnlyProperty("total", &ProgressEvent::total);
}

}  // namespace events
}  // namespace js
}  // namespace shaka
