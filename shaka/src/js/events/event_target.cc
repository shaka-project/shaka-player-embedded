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

#include "src/js/events/event_target.h"

#include <vector>

#include "src/js/js_error.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace events {

EventTarget::EventTarget() : is_dispatching_(false) {}
// \cond Doxygen_Skip
EventTarget::~EventTarget() {}
// \endcond Doxygen_Skip

void EventTarget::Trace(memory::HeapTracer* tracer) const {
  BackingObject::Trace(tracer);
  for (auto& listener : listeners_) {
    tracer->Trace(&listener.callback_);
  }
  for (auto& pair : on_listeners_) {
    tracer->Trace(pair.second);
  }
}

void EventTarget::AddEventListener(const std::string& type, Listener callback) {
  if (FindListener(callback, type) != listeners_.end())
    return;
  listeners_.emplace_back(callback, type);
}

void EventTarget::SetCppEventListener(EventType type,
                                      std::function<void()> callback) {
  cpp_listeners_.emplace(to_string(type), callback);
}

void EventTarget::RemoveEventListener(const std::string& type,
                                      Listener callback) {
  auto it = FindListener(callback, type);
  if (it != listeners_.end()) {
    if (is_dispatching_) {
      it->should_remove_ = true;
    } else {
      listeners_.erase(it);
    }
  }
}

void EventTarget::UnsetCppEventListener(EventType type) {
  cpp_listeners_.erase(to_string(type));
}

ExceptionOr<bool> EventTarget::DispatchEvent(RefPtr<Event> event) {
  if (is_dispatching_) {
    return JsError::DOMException(InvalidStateError,
                                 "Already dispatching events.");
  }

  is_dispatching_ = true;

  event->target = this;

  // Shaka Player does not use capturing or bubbling events, so we only care
  // about the initial target.  Normally we would need to construct a path
  // going up the DOM.
  event->event_phase = Event::AT_TARGET;
  InvokeListeners(event);

  // Now that we are done firing events, remove the event listeners that have
  // been marked for removal.
  for (auto it = listeners_.begin(); it != listeners_.end();) {
    if (it->should_remove_)
      it = listeners_.erase(it);
    else
      ++it;
  }

  is_dispatching_ = false;
  event->event_phase = Event::NONE;
  event->current_target = nullptr;
  return !event->default_prevented;
}

EventTarget::ListenerInfo::ListenerInfo(Listener listener,
                                        const std::string& type)
    : callback_(listener), type_(type), should_remove_(false) {}

EventTarget::ListenerInfo::~ListenerInfo() {}

void EventTarget::InvokeListeners(RefPtr<Event> event) {
  if (event->is_stopped())
    return;

  event->current_target = this;

  // First, evoke the cpp callbacks.  They have priority, due to being internal.
  // It is assumed that they will not change during this process.
  if (cpp_listeners_.count(event->type) > 0)
    cpp_listeners_.at(event->type)();

  // Invoke the on-event listeners second.  This is slightly different from
  // Chrome which will invoke it in the order it was set (i.e. calling
  // addEventListener then setting onerror will call callbacks in that order).
  auto on_iter = on_listeners_.find(event->type);
  if (on_iter != on_listeners_.end()) {
    // Note that even though it exists in the map does not mean the field is
    // set.
    if (on_iter->second->has_value()) {
      on_iter->second->value().CallWithThis(this, event);
      if (event->is_immediate_stopped()) {
        return;
      }
    }
  }

  if (listeners_.empty())
    return;

  // Store the end of the list.  Elements are added to the end of the list so
  // we will not fire events added after this one.  This needs to be inclusive
  // since the pseudo-value for end() may not remain valid after insertions.
  auto end = --listeners_.end();

  // We need to process all items in the range [begin, end].  We cannot use a
  // for loop because we still need to process the |end| element.  So we process
  // |it|, then increment, then compare the old value to |end|.  Note that
  // since |end| is valid, the last increment is still valid.
  auto it = listeners_.begin();
  do {
    if (it->should_remove_)
      continue;

    if (it->type_ == event->type && it->callback_.has_value()) {
      it->callback_->CallWithThis(this, event);
    }

    if (event->is_immediate_stopped())
      break;
  } while ((it++) != end);
}

std::list<EventTarget::ListenerInfo>::iterator EventTarget::FindListener(
    const Listener& callback, const std::string& type) {
  auto it = listeners_.begin();
  for (; it != listeners_.end(); it++) {
    if (it->type_ == type && callback == it->callback_) {
      break;
    }
  }
  return it;
}

EventTargetFactory::EventTargetFactory() {
  AddMemberFunction("addEventListener", &EventTarget::AddEventListener);
  AddMemberFunction("removeEventListener", &EventTarget::RemoveEventListener);
  AddMemberFunction("dispatchEvent", &EventTarget::DispatchEvent);
}

}  // namespace events
}  // namespace js
}  // namespace shaka
