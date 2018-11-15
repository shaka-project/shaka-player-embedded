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

#ifndef SHAKA_EMBEDDED_CORE_WEAK_JS_PTR_H_
#define SHAKA_EMBEDDED_CORE_WEAK_JS_PTR_H_

#include <glog/logging.h>

#include <type_traits>

#include "src/mapping/js_wrappers.h"
#include "src/memory/heap_tracer.h"

namespace shaka {

/**
 * Represents a weak pointer to a JavaScript value.  When the GC runs, when
 * this is traced it will mark the value alive so it is not freed.  If this is
 * not traced, the GC will free the value.
 *
 * This MUST be traced.  If the GC runs and the object is freed, this object
 * may become invalid.  JSC does not keep track of weak pointers, so
 * this must be traced to remain valid.
 *
 * This is NOT a GenericConverter and should not be used as arguments, fields,
 * etc..  This should only be used in the mapping types to store JavaScript
 * objects.
 */
template <typename T>
class WeakJsPtr : public memory::Traceable {
 public:
  WeakJsPtr() {}

  WeakJsPtr(std::nullptr_t) {}  // NOLINT(runtime/explicit)

  template <typename U = T>
  WeakJsPtr(Handle<U> other) {  // NOLINT(runtime/explicit)
    ResetInternal(other);
  }

  WeakJsPtr(const WeakJsPtr& other) {
    // Need to define an explicit copy-constructor since v8::Global is
    // non-copyable.
    ResetInternal(other.ptr_);
  }

  WeakJsPtr(WeakJsPtr&& other) {
    // There are problems using the v8::Global<T> move constructor.  So just
    // copy and clear the original.
    ResetInternal(other.ptr_);
    other.reset();
  }

  ~WeakJsPtr() override {
#ifdef USING_V8
    // V8 tracks the address of |ptr_| so it can reset it once the object is
    // cleared.  We need to clear this so V8 doesn't modify other memory.
    if (!empty()) {
      DCHECK(ptr_.IsWeak());
      ptr_.ClearWeak();
    }
#endif
  }


  WeakJsPtr& operator=(const WeakJsPtr& other) {
    ResetInternal(other.ptr_);
    return *this;
  }
  WeakJsPtr& operator=(WeakJsPtr&& other) {
    // There are problems using the v8::Global<T> move constructor.  So just
    // copy and clear the original.
    ResetInternal(other.ptr_);
    other.reset();
    return *this;
  }

  template <typename U = T>
  bool operator==(const WeakJsPtr<U>& other) const {
    return ptr_ == other.ptr_;
  }
  template <typename U = T>
  bool operator==(const Handle<U>& other) const {
    return ptr_ == other;
  }
  template <typename U = T>
  bool operator!=(const WeakJsPtr<U>& other) const {
    return ptr_ != other.ptr_;
  }
  template <typename U = T>
  bool operator!=(const Handle<U>& other) const {
    return ptr_ != other;
  }

  /** @return Whether the pointer is empty. */
  bool empty() const {
#if defined(USING_V8)
    return ptr_.IsEmpty();
#elif defined(USING_JSC)
    return !ptr_;
#endif
  }

  /** @return A local handle to the weak pointer. */
  Handle<T> handle() const {
    CHECK(!empty());
#if defined(USING_V8)
    return ptr_.Get(GetIsolate());
#elif defined(USING_JSC)
    return ptr_;
#endif
  }

  /** @return A value containing the stored value. */
  ReturnVal<JsValue> value() const {
    return RawToJsValue(handle());
  }

  void reset() {
#if defined(USING_V8)
    ptr_.Reset();
#elif defined(USING_JSC)
    ptr_ = nullptr;
#endif
  }
  template <typename U>
  void reset(Handle<U> other) {
    ResetInternal(other);
  }


  void Trace(memory::HeapTracer* tracer) const override {
    if (!empty()) {
#if defined(USING_V8)
#  ifndef NDEBUG
      // Create a handle scope so the handle local() is freed when this returns.
      v8::HandleScope handles(GetIsolate());
      CHECK(empty() || !handle()->IsNumber());
#  endif

      ptr_.RegisterExternalReference(GetIsolate());
#endif
    }
  }

 private:
  template <typename>
  friend class WeakJsPtr;

  template <typename U>
  void ResetInternal(const U& other) {
#if defined(USING_V8)
    // Don't do anything if they are both empty.  This allows using WeakJsPtr
    // in a background thread where GetIsolate() would fail a CHECK.
    if (ptr_.IsEmpty() && other.IsEmpty())
      return;

    // Mark the Global as weak so the object can be destroyed even though we
    // hold a pointer.  If |this| is alive, the HeapTracer will call MarkAlive.
    ptr_.Reset(GetIsolate(), other);
    if (!ptr_.IsEmpty())
      ptr_.SetWeak();
#else
    ptr_ = other;
#endif
  }

#if defined(USING_V8)
  v8::Global<T> ptr_;
#elif defined(USING_JSC)
  Handle<T> ptr_;
#endif
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_CORE_WEAK_JS_PTR_H_
