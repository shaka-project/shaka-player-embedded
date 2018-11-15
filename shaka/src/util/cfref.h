// Copyright 2017 Google LLC
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

#ifndef SHAKA_EMBEDDED_UTIL_CF_REF_H_
#define SHAKA_EMBEDDED_UTIL_CF_REF_H_

#ifdef __APPLE__

#  include <CoreFoundation/CoreFoundation.h>

#  include <memory>
#  include <type_traits>

namespace shaka {
namespace util {

/**
 * A type traits type that is used to get information about the given ref type.
 * This can be specialized for other types if needed.
 */
template <typename T>
struct RefTypeTraits {
  /**
   * Whether a raw pointer needs to be Duplicated.  If false, the pointer is
   * assumed to already been ref-counted.
   */
  static constexpr const bool AcquireWithRaw = false;

  /**
   * Duplicates the given pointer and returns the new pointer.  This MUST
   * be able to accept nullptr.
   */
  static T Duplicate(T arg) {
    if (arg)
      CFRetain(arg);
    return arg;
  }

  /**
   * Releases the given pointer.  This MUST be able to accept nullptr.
   */
  static void Release(T arg) {
    if (arg)
      CFRelease(arg);
  }
};


/**
 * Base class for a type that handles Apple's memory management.  For example,
 * this can handle the reference for a CFString.  This is copyable, move-aware,
 * and is RAII (will free the reference at the end).
 */
template <typename T>
class CFRef {
 public:
  using traits = RefTypeTraits<T>;
  using value_type = T;

  CFRef() : ptr_(nullptr) {}
  CFRef(std::nullptr_t) : ptr_(nullptr) {}  // NOLINT(runtime/explicit)
  CFRef(T arg)  // NOLINT(runtime/explicit)
      : ptr_(traits::AcquireWithRaw ? traits::Duplicate(arg) : arg) {}
  CFRef(const CFRef& other) : ptr_(traits::Duplicate(other.ptr_)) {}
  CFRef(CFRef&& other) : ptr_(other.Detach()) {}
  // Careful, these assume that if U is compatible with T, that the
  // Duplicate/Release methods are compatible.
  template <typename U>
  CFRef(const CFRef<U>& other) : ptr_(traits::Duplicate(other.ptr_)) {}
  template <typename U>
  CFRef(CFRef<U>&& other) : ptr_(other.Detach()) {}
  ~CFRef() {
    Release();
  }

  CFRef& operator=(const CFRef& other) {
    Release();
    ptr_ = traits::Duplicate(other);
    return *this;
  }
  CFRef& operator=(CFRef&& other) {
    Release();
    ptr_ = other.Detach();
    return *this;
  }
  CFRef& operator=(T other) {
    Release();
    ptr_ = traits::AcquireWithRaw ? traits::Duplicate(other) : other;
    return *this;
  }

  /**
   * Implicit conversion to the pointer type.  This is so you can pass the
   * reference as an argument to native methods.  This does NOT increase the
   * ref-count, so the result should NOT be stored in another variable.  Use
   * the normal copy-construction to duplicate the reference.
   */
  operator T() const {
    return ptr_;
  }

  /**
   * Detaches the pointer from this object.  The returned pointer will still
   * be ref-counted and it is up to the calling code to free it.
   */
  T Detach() {
    T ret = ptr_;
    ptr_ = nullptr;
    return ret;
  }


  /**
   * Creates a new reference that increase the ref count.  This should be used
   * for pointers that are not owned by calling code but wants to be kept,
   * for example, from non-Create methods.
   */
  static CFRef Acquire(T arg) {
    if (traits::AcquireWithRaw)
      return CFRef(arg);
    else
      return CFRef(traits::Duplicate(arg));
  }

 private:
  template <typename U>
  friend class CFRef;

  void Release() {
    traits::Release(ptr_);
    ptr_ = nullptr;
  }

  T ptr_;
};

}  // namespace util
}  // namespace shaka

#endif  // __APPLE__

#endif  // SHAKA_EMBEDDED_UTIL_CF_REF_H_
