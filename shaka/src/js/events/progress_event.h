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

#ifndef SHAKA_EMBEDDED_JS_EVENTS_PROGRESS_EVENT_H_
#define SHAKA_EMBEDDED_JS_EVENTS_PROGRESS_EVENT_H_

#include <string>

#include "src/js/events/event.h"
#include "src/mapping/backing_object_factory.h"

namespace shaka {
namespace js {
namespace events {

class ProgressEvent : public Event {
  DECLARE_TYPE_INFO(ProgressEvent);

 public:
  ProgressEvent(EventType, bool, double, double);

  static ProgressEvent* Create(const std::string& type) {
    return new ProgressEvent(type, false, 0, 0);
  }

  const bool length_computable;
  const double loaded;
  const double total;

 private:
  ProgressEvent(const std::string& type, bool length_computable, double loaded,
                double total);
};

class ProgressEventFactory : public BackingObjectFactory<ProgressEvent, Event> {
 public:
  ProgressEventFactory();
};

}  // namespace events
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_EVENTS_PROGRESS_EVENT_H_
