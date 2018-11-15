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

#ifndef SHAKA_EMBEDDED_CORE_MEMBER_H_
#define SHAKA_EMBEDDED_CORE_MEMBER_H_

#include <glog/logging.h>

#include <string>

#include "src/core/ref_ptr.h"
#include "src/mapping/convert_js.h"
#include "src/mapping/generic_converter.h"
#include "src/mapping/js_wrappers.h"
#include "src/mapping/names.h"
#include "src/memory/heap_tracer.h"
#include "src/memory/object_tracker.h"
#include "src/util/templates.h"

namespace shaka {

/**
 * Defines a smart pointer type used to store references to GC types.
 *
 * Note that when a field is of type Member<T>, it can be empty; this means the
 * object is equal to |null| or was assigned to either |null| or |undefined|.
 */
template <typename T>
class Member : public GenericConverter, public memory::Traceable {
  static_assert(!util::is_v8_type<T>::value,
                "Cannot use Member<T> for JavaScript objects.");

 public:
  static std::string name() {
    return TypeName<T>::name();
  }

  Member() {}

  Member(std::nullptr_t) {}  // NOLINT(runtime/explicit)

  template <typename U>
  Member(U* other) {  // NOLINT(runtime/explicit)
    reset(other);
  }

  Member(const Member& other) {
    reset(other.ptr_);
  }

  template <typename U>
  Member(const Member<U>& other) {
    reset(other.ptr_);
  }

  Member(Member&& other) {
    reset(other.ptr_);
    other.ptr_ = nullptr;
  }

  template <typename U>
  Member(Member<U>&& other) {
    reset(other.ptr_);
    other.ptr_ = nullptr;
  }

  template <typename U>
  Member(const RefPtr<U>& other) {  // NOLINT(runtime/explicit)
    reset(other.get());
  }

  template <typename U>
  Member(RefPtr<U>&& other) {  // NOLINT(runtime/explicit)
    reset(other.get());
    other.reset();
  }

  ~Member() override {
    // Do not free |ptr_| since it is handled by ObjectTracker.
  }


  Member& operator=(const Member& other) {
    reset(other.ptr_);
    return *this;
  }
  Member& operator=(Member&& other) {
    reset(other.ptr_);
    other.ptr_ = nullptr;
    return *this;
  }

  T& operator*() const {
    CHECK(ptr_);
    return *ptr_;
  }
  T* operator->() const {
    CHECK(ptr_);
    return ptr_;
  }
  operator T*() const {
    return ptr_;
  }

  /** @return Whether the pointer is empty. */
  bool empty() const {
    return ptr_ == nullptr;
  }

  /** @return The pointer this holds, may be nullptr. */
  T* get() const {
    return ptr_;
  }

  template <typename U = T>
  void reset(U* other = nullptr) {
    ptr_ = other;
    // Force the given pointer to be alive.  This is required if the GC run is
    // currently going on and the parent object has already been traced.  If
    // that is the case, this pointer may not be considered alive.  The only way
    // to call this method is if the parent is alive, so |other| must be
    // alive too.
    if (other)
      memory::ObjectTracker::Instance()->ForceAlive(other);
  }

  bool TryConvert(Handle<JsValue> source) override {
    RefPtr<T> local_dest;
    if (!FromJsValue(source, &local_dest))
      return false;
    ptr_ = local_dest;
    return true;
  }

  ReturnVal<JsValue> ToJsValue() const override {
    if (empty())
      return JsNull();
    else
      return ptr_->JsThis();
  }

  void Trace(memory::HeapTracer* tracer) const override {
    tracer->Trace(ptr_);
  }

 private:
  T* ptr_ = nullptr;
};


// RefPtr constructors, defined here to avoid a circular header dependency.
template <typename T>
template <typename U>
RefPtr<T>::RefPtr(const Member<U>& other) {
  reset(other.get());
}

template <typename T>
template <typename U>
RefPtr<T>::RefPtr(Member<U>&& other) {
  reset(other.get());
  other.reset();
}

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_CORE_MEMBER_H_
