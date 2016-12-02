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

#ifndef SHAKA_EMBEDDED_JS_EVENTS_EVENT_TARGET_H_
#define SHAKA_EMBEDDED_JS_EVENTS_EVENT_TARGET_H_

#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "shaka/optional.h"
#include "src/core/js_manager_impl.h"
#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/debug/thread_event.h"
#include "src/js/events/event.h"
#include "src/js/events/event_names.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/callback.h"
#include "src/mapping/exception_or.h"

namespace shaka {
namespace js {
namespace events {

class EventTarget : public BackingObject {
  DECLARE_TYPE_INFO(EventTarget);

 public:
  EventTarget();

  using Listener = optional<Callback>;

  void Trace(memory::HeapTracer* tracer) const override;

  /**
   * Adds a js-backed event listener to the target.
   * @param type The name of the event to listen for.
   * @param callback The listener for the event.  Must be a function.
   */
  void AddEventListener(const std::string& type, Listener callback);

  /**
   * Adds a not-js-backed event listener to the target.
   * There should only be one such event listener per event.
   * @param type The name of the event to listen for.
   * @param callback The listener for the event.  Must be a function.
   */
  void SetCppEventListener(EventType type, std::function<void()> callback);

  /**
   * Removes a js-backed event listener from the target.
   * @param type The name of the event to remove.
   * @param callback The listener to remove.
   */
  void RemoveEventListener(const std::string& type, Listener callback);

  /**
   * Removes a not-js-backed event listener from the target.
   * @param type The name of the event to remove.
   */
  void UnsetCppEventListener(EventType type);

  /**
   * Dispatches the event to the current object.  This method blocks until the
   * event is complete.  The event is treated as not trusted even if it comes
   * from internal code (Shaka Player doesn't care).  This must be called from
   * the event thread.
   * @param event The event to dispatch.
   * @return False if one listener called preventDefault, otherwise true.
   */
  ExceptionOr<bool> DispatchEvent(RefPtr<Event> event) {
    return DispatchEventInternal(event, nullptr);
  }

  /**
   * @see DispatchEvent
   * @param event The event to dispatch
   * @param did_listeners_throw [OUT] If given, will be set to whether a
   *   listener threw an exception.
   * @return False if one listener called preventDefault, otherwise true.
   */
  ExceptionOr<bool> DispatchEventInternal(RefPtr<Event> event,
                                          bool* did_listeners_throw);

  /**
   * Asynchronously raises the given event on this.  It is safe to call this
   * from any thread.  There needs to be an explicit type parameter for the type
   * of event to raise, the remaining types should be arguments to its
   * constructor.  The constructor used does not need to be the one used from
   * JavaScript.
   */
  template <typename EventType, typename... Args>
  std::shared_ptr<ThreadEvent<bool>> ScheduleEvent(Args&&... args) {
    RefPtr<EventType> event = new EventType(std::forward<Args>(args)...);
    return JsManagerImpl::Instance()->MainThread()->AddInternalTask(
        TaskPriority::Events, std::string("Schedule ") + EventType::name(),
        ScheduleEventTask<EventType>(this, event));
  }

  /**
   * Synchronously raises the given event on this.  This must only be called
   * from the event thread.
   */
  template <typename EventType, typename... Args>
  ExceptionOr<bool> RaiseEvent(Args... args) {
    RefPtr<EventType> backing = new EventType(args...);
    return this->DispatchEvent(backing);
  }

 protected:
  /** Registers an event on the target. */
  void AddListenerField(EventType type, Listener* on_field) {
    on_listeners_[to_string(type)] = on_field;
  }

 private:
  template <typename E>
  class ScheduleEventTask : public memory::Traceable {
   public:
    ScheduleEventTask(EventTarget* target, RefPtr<E> event)
        : target_(target), event_(event) {}
    ~ScheduleEventTask() override {}

    void Trace(memory::HeapTracer* tracer) const override {
      tracer->Trace(&target_);
      tracer->Trace(&event_);
    }

    bool operator()() {
      ExceptionOr<bool> val = target_->DispatchEvent(event_);
      if (holds_alternative<bool>(val)) {
        return get<bool>(val);
      } else {
        LocalVar<JsValue> except = get<js::JsError>(val).error();
        LOG(INFO) << "Exception thrown while raising event: "
                  << ConvertToString(except);
        return false;
      }
    }

   private:
    Member<EventTarget> target_;
    Member<E> event_;
  };

  struct ListenerInfo {
    ListenerInfo(Listener listener, const std::string& type);
    ~ListenerInfo();

    Listener callback_;
    std::string type_;
    bool should_remove_;
  };

  /** Invokes all the listeners for the given event */
  void InvokeListeners(RefPtr<Event> event, bool* did_listeners_throw);

  /** Finds the listener info that matches the given callback. */
  std::list<ListenerInfo>::iterator FindListener(const Listener& callback,
                                                 const std::string& type);

  std::unordered_map<std::string, std::function<void()>> cpp_listeners_;

  // Elements are stored in insert order. Use a list since we store an iterator
  // while invoking and it needs to remain valid with concurrent inserts.  This
  // will also have faster removals since we only use iterators.
  std::list<ListenerInfo> listeners_;
  // A map of the on-event listeners (e.g. onerror).
  std::unordered_map<std::string, Listener*> on_listeners_;
  bool is_dispatching_;
};

class EventTargetFactory : public BackingObjectFactory<EventTarget> {
 public:
  EventTargetFactory();
};

}  // namespace events
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_EVENTS_EVENT_TARGET_H_
