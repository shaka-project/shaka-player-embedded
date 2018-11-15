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

#ifndef SHAKA_EMBEDDED_CORE_REF_PTR_H_
#define SHAKA_EMBEDDED_CORE_REF_PTR_H_

#include <glog/logging.h>

#include <iostream>
#include <string>

#include "src/mapping/convert_js.h"
#include "src/mapping/js_wrappers.h"
#include "src/mapping/names.h"
#include "src/memory/object_tracker.h"
#include "src/util/templates.h"

namespace shaka {

template <typename T>
class Member;

/**
 * Defines a smart pointer that performs ref counting on the value that is
 * stored.
 *
 * Note that when an argument is of type RefPtr<T>, it can be empty; this means
 * the argument is either |null| or |undefined|.  Returning an empty RefPtr
 * means returning |null|.
 */
template <typename T>
class RefPtr {
  static_assert(!util::is_v8_type<T>::value, "Cannot be used on V8 types.");

 public:
  static std::string name() {
    return TypeName<T>::name();
  }

  RefPtr() {}

  RefPtr(std::nullptr_t) {}  // NOLINT(runtime/explicit)

  template <typename U>
  RefPtr(U* other) {  // NOLINT(runtime/explicit)
    reset(other);
  }

  // Copy-constructors cannot have a template, even if it can be deduced.
  RefPtr(const RefPtr& other) {
    reset(other.ptr_);
  }

  template <typename U>
  RefPtr(const RefPtr<U>& other) {
    reset(other.ptr_);
  }

  // Move-constructors cannot have a template, even if it can be deduced.
  RefPtr(RefPtr&& other) {
    reset(other.ptr_);
    other.reset();
  }

  template <typename U>
  RefPtr(RefPtr<U>&& other) {
    reset(other.ptr_);
    other.reset();
  }

  ~RefPtr() {
    reset();
  }

  // Defined in member.h to avoid circular dependencies in the headers.
  template <typename U>
  RefPtr(const Member<U>& other);  // NOLINT(runtime/explicit)
  template <typename U>
  RefPtr(Member<U>&& other);  // NOLINT(runtime/explicit)


  // Note this still supports other kinds of assignment because the constructors
  // are not explicit.  So when trying to assign a U*, it will first call
  // the constructor, then the move constructor.  This is less efficient, but
  // avoids a lot of code duplication.
  RefPtr& operator=(const RefPtr& other) {
    reset(other.ptr_);
    return *this;
  }
  RefPtr& operator=(RefPtr&& other) {
    reset(other.ptr_);
    other.reset();
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
    // Add to other first in case other == ptr_, we don't want the GC to collect
    // the object in between the calls.
    static_assert(std::is_convertible<U*, T*>::value,
                  "U must be implicitly convertible to T");
    memory::ObjectTracker::Instance()->AddRef(other);
    memory::ObjectTracker::Instance()->RemoveRef(ptr_);
    ptr_ = other;
  }

 private:
  template <typename U>
  friend class RefPtr;

  T* ptr_ = nullptr;
};

template <typename T>
std::ostream& operator<<(std::ostream& os, RefPtr<T> arg) {
  if (arg.empty())
    return os << "(NULL)";
  else
    return os << arg.get();
}

template <typename T>
struct impl::ConvertHelper<RefPtr<T>> {
  static bool FromJsValue(Handle<JsValue> source, RefPtr<T>* dest) {
    if (IsNullOrUndefined(source)) {
      dest->reset();
      return true;
    }

    // Get backing pointer and verify the type of the backing is correct.
    BackingObject* ptr = GetInternalPointer(source);
    if (!IsDerivedFrom(ptr, TypeName<T>::name()))
      return false;

    dest->reset(static_cast<T*>(ptr));
    return true;
  }

  static ReturnVal<JsValue> ToJsValue(RefPtr<T> source) {
    if (source.empty())
      return JsNull();
    else
      return source->JsThis();
  }
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_CORE_REF_PTR_H_
