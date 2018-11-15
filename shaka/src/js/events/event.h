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

#ifndef SHAKA_EMBEDDED_JS_EVENTS_EVENT_H_
#define SHAKA_EMBEDDED_JS_EVENTS_EVENT_H_

#include <string>

#include "src/core/member.h"
#include "src/js/events/event_names.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"

namespace shaka {
namespace js {
namespace events {

class EventTarget;

class Event : public BackingObject {
  DECLARE_TYPE_INFO(Event);

 protected:
  explicit Event(const std::string& type);

 public:
  enum EventPhase {
    NONE = 0,
    CAPTURING_PHASE = 1,
    AT_TARGET = 2,
    BUBBLING_PHASE = 3,
  };

  explicit Event(EventType type);

  static Event* Create(const std::string& type) {
    return new Event(type);
  }

  void Trace(memory::HeapTracer* tracer) const override;
  bool IsShortLived() const override;

  bool is_stopped() const {
    return stop_propagation_;
  }
  bool is_immediate_stopped() const {
    return stop_immediate_propagation_;
  }

  // Exposed methods.
  void PreventDefault();
  void StopImmediatePropagation();
  void StopPropagation();

  // Exposed fields.
  const bool bubbles = false;
  const bool cancelable = false;
  const bool is_trusted = false;
  const std::string type;
  const double time_stamp;
  Member<EventTarget> current_target;
  Member<EventTarget> target;
  int event_phase = NONE;
  bool default_prevented = false;

 private:
  bool stop_propagation_ = false;
  bool stop_immediate_propagation_ = false;
};

class EventFactory : public BackingObjectFactory<Event> {
 public:
  EventFactory();
  ~EventFactory() override;
};

}  // namespace events
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_EVENTS_EVENT_H_
