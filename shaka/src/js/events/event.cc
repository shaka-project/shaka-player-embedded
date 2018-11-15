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

#include "src/js/events/event.h"

#include "src/js/dom/document.h"
#include "src/js/events/event_target.h"
#include "src/memory/heap_tracer.h"
#include "src/util/clock.h"

namespace shaka {
namespace js {
namespace events {

Event::Event(EventType type) : Event(to_string(type)) {}

Event::Event(const std::string& type)
    : type(type),
      time_stamp(util::Clock::Instance.GetMonotonicTime() -
                 dom::Document::GetGlobalDocument()->created_at()) {}

// \cond Doxygen_Skip
Event::~Event() {}
// \endcond Doxygen_Skip

void Event::Trace(memory::HeapTracer* tracer) const {
  BackingObject::Trace(tracer);
  tracer->Trace(&current_target);
  tracer->Trace(&target);
}

bool Event::IsShortLived() const {
  return true;
}

void Event::PreventDefault() {
  if (cancelable)
    default_prevented = true;
}

void Event::StopImmediatePropagation() {
  stop_propagation_ = true;
  stop_immediate_propagation_ = true;
}

void Event::StopPropagation() {
  stop_propagation_ = true;
}


EventFactory::EventFactory() {
  AddReadOnlyProperty("bubbles", &Event::bubbles);
  AddReadOnlyProperty("cancelable", &Event::cancelable);
  AddReadOnlyProperty("currentTarget", &Event::current_target);
  AddReadOnlyProperty("defaultPrevented", &Event::default_prevented);
  AddReadOnlyProperty("eventPhase", &Event::event_phase);
  AddReadOnlyProperty("target", &Event::target);
  AddReadOnlyProperty("timeStamp", &Event::time_stamp);
  AddReadOnlyProperty("type", &Event::type);
  AddReadOnlyProperty("isTrusted", &Event::is_trusted);

  AddMemberFunction("preventDefault", &Event::PreventDefault);
  AddMemberFunction("stopImmediatePropagation",
                    &Event::StopImmediatePropagation);
  AddMemberFunction("stopPropagation", &Event::StopPropagation);
}
EventFactory::~EventFactory() {}

}  // namespace events
}  // namespace js
}  // namespace shaka
