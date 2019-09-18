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

#ifndef SHAKA_EMBEDDED_MEMORY_HEAP_TRACER_H_
#define SHAKA_EMBEDDED_MEMORY_HEAP_TRACER_H_

#include <glog/logging.h>

#include <list>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "shaka/optional.h"
#include "shaka/variant.h"
#include "src/debug/mutex.h"
#include "src/util/templates.h"

namespace shaka {
class BackingObject;

namespace memory {
class HeapTracer;

/**
 * Defines an object that can be traced by the HeapTracer.  Any object that
 * stores other Traceable objects (e.g. BackingObjects or GenericConverters)
 * MUST be Traceable so we can trace the heap.
 */
class Traceable {
 public:
  /**
   * The duration, in milliseconds, that an object is expected to remain
   * alive once its C++ ref-count hits zero.
   */
  static constexpr const uint64_t kShortLiveDurationMs = 5000;


  virtual ~Traceable() {}

  /**
   * Called during a GC run.  This should call HeapTracer::Trace on all
   * Traceable members.  Be sure to call the base method when overriding.
   */
  virtual void Trace(HeapTracer* tracer) const = 0;

  /**
   * Gets whether this object is defined to be alive because of a JavaScript
   * root reference.
   */
  virtual bool IsRootedAlive() const;

  /**
   * Gets whether the object is considered short-lived.  This means that once
   * the C++ ref-count is zero, the object won't remain alive for long.  It
   * is important to only set this if the JavaScript object won't be used for
   * long.
   *
   * This exists for JSC which doesn't offer a way for us to track whether a
   * JavaScript object is still alive.  If the JavaScript object is used after
   * the backing object is destroyed, then a JavaScript exception will be
   * thrown.
   */
  virtual bool IsShortLived() const;
};


/**
 * This is used to trace our heap to mark objects as alive and tell the
 * JavaScript engine of references we hold.
 */
class HeapTracer {
 public:
  HeapTracer();
  ~HeapTracer();

  /** @return A set of all the alive objects for this GC pass. */
  const std::unordered_set<const Traceable*>& alive() const {
    return alive_;
  }

  /**
   * Forces the given pointer to be marked as alive for the current GC run. This
   * ensures that when assigning to a Member<T> field in the middle of a GC run,
   * the object will not be lost.
   */
  void ForceAlive(const Traceable* ptr);

  /**
   * Called from the Traceable::Trace method.  This marks the given member as
   * alive and recursively marks child objects as alive.
   */
  void Trace(const Traceable* ptr);

  template <typename T>
  void Trace(const std::vector<T>* array) {
    for (const T& item : *array) {
      Trace(&item);
    }
  }
  template <typename T>
  void Trace(const std::list<T>* array) {
    for (const T& item : *array) {
      Trace(&item);
    }
  }
  template <typename T>
  void Trace(const optional<T>* opt) {
    if (opt->has_value())
      Trace(&opt->value());
  }
  template <typename... Types>
  void Trace(const variant<Types...>* variant) {
    VariantHelper<0, Types...>::Trace(this, variant);
  }

  // Allow passing other types, just ignore it.  This simplifies generic
  // converter templates so they don't have to check if the type is special
  // or not.
  void Trace(const std::string*) {}
  void Trace(const bool*) {}
  template <typename T,
            typename = util::enable_if_t<util::is_number<T>::value ||
                                         std::is_enum<T>::value>>
  void Trace(const T*) {}

  /** Begins a new GC pass. */
  void BeginPass();

  /**
   * Traces common objects, including the given ref-counted alive objects.  This
   * MUST be called at least once each GC pass.
   */
  void TraceCommon(const std::unordered_set<const Traceable*> ref_alive);

  /** Resets the stored state. */
  void ResetState();

 private:
  template <size_t I, typename... Types>
  struct VariantHelper {
    static void Trace(HeapTracer* tracer, const variant<Types...>* variant) {
      if (variant->index() == I)
        tracer->Trace(&get<I>(*variant));
      else
        VariantHelper<I + 1, Types...>::Trace(tracer, variant);
    }
  };
  template <typename... Types>
  struct VariantHelper<sizeof...(Types), Types...> {
    static void Trace(HeapTracer* tracer, const variant<Types...>* variant) {
      CHECK_EQ(variant->index(), sizeof...(Types) - 1);
      tracer->Trace(&get<sizeof...(Types) - 1>(*variant));
    }
  };

  Mutex mutex_;
  std::unordered_set<const Traceable*> alive_;
  std::unordered_set<const Traceable*> pending_;
};

}  // namespace memory
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEMORY_HEAP_TRACER_H_
