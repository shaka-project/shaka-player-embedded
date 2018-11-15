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

#ifndef SHAKA_EMBEDDED_UTIL_PSEUDO_SINGLETON_H_
#define SHAKA_EMBEDDED_UTIL_PSEUDO_SINGLETON_H_

#include <glog/logging.h>

#include <atomic>
#include <type_traits>

#include "src/util/macros.h"

namespace shaka {

/**
 * A base class for types that should only have one instance at a time.  This
 * defines a static method to get the instance that will be inherited to the
 * type.  This type is fully thread-safe and can be used on any thread.
 *
 * The type argument should be the type itself.  For example:
 *
 * class Foo : public PseudoSingleton<Foo> {};
 */
template <typename T>
class PseudoSingleton {
 public:
  /**
   * An RAII type that unsets the singleton for this type for the duration
   * of this object's lifetime.
   */
  struct UnsetForTesting {
    UnsetForTesting()
        : value_(instance_.exchange(nullptr, std::memory_order_acq_rel)) {}

    ~UnsetForTesting() {
      T* expected = nullptr;
      CHECK(instance_.compare_exchange_strong(expected, value_,
                                              std::memory_order_acq_rel));
    }

   private:
    T* value_;
  };

  // Since the base class is initialized first, the virtual pointers in *this
  // haven't been setup yet.  This means that ubsan will tell us this is an
  // invalid cast.
  NO_SANITIZE("vptr")
  PseudoSingleton() {
    T* expected = nullptr;
    CHECK(instance_.compare_exchange_strong(expected, static_cast<T*>(this),
                                            std::memory_order_acq_rel));
  }

  ~PseudoSingleton() {
    T* expected = static_cast<T*>(this);
    CHECK(instance_.compare_exchange_strong(expected, nullptr,
                                            std::memory_order_acq_rel));
  }

  static T* Instance() {
    T* ret = instance_.load(std::memory_order_acquire);
    CHECK(ret);
    return ret;
  }

  static T* InstanceOrNull() {
    return instance_.load(std::memory_order_acquire);
  }

 private:
  static std::atomic<T*> instance_;
};

template <typename T>
std::atomic<T*> PseudoSingleton<T>::instance_{nullptr};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTIL_PSEUDO_SINGLETON_H_
