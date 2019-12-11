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

#ifndef SHAKA_EMBEDDED_MEMORY_OBJECT_TRACKER_H_
#define SHAKA_EMBEDDED_MEMORY_OBJECT_TRACKER_H_

#include <glog/logging.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/debug/mutex.h"
#include "src/util/pseudo_singleton.h"
#include "src/util/macros.h"
#include "src/util/templates.h"

namespace shaka {

class JsManagerImpl;
class RefPtrTest;

namespace eme {
class ClearKeyImplementationTest;
}  // namespace eme

namespace memory {

class HeapTracer;
class Traceable;

/**
 * Defines a dynamic object tracker.  This is a singleton class.  This is used
 * to track the dynamic backing objects that we create so we can free them when
 * they are no longer used.  Deriving from BackingObjectBase will automatically
 * use this as the backing store for 'new' usages.  Objects allocated using this
 * should not use 'delete'.
 */
class ObjectTracker final : public PseudoSingleton<ObjectTracker> {
 public:
  explicit ObjectTracker(HeapTracer* tracer);
  ~ObjectTracker();

  NON_COPYABLE_OR_MOVABLE_TYPE(ObjectTracker);

  /** Registers the given object to be tracked. */
  void RegisterObject(Traceable* object);

  /** @see HeapTracer::ForceAlive */
  void ForceAlive(const Traceable* ptr);

  /** Increment the reference count of the given object. */
  void AddRef(const Traceable* object);

  /** Decrement the reference count of the given object. */
  void RemoveRef(const Traceable* object);

  /** Get all the objects that have a non-zero ref count. */
  std::unordered_set<const Traceable*> GetAliveObjects() const;

  /**
   * Called from the HeapTracer to free objects during a GC run.
   * @param alive A set of all the currently alive objects.
   */
  void FreeDeadObjects(const std::unordered_set<const Traceable*>& alive);

  /** Releases all object this owns.  This is called as part of shutdown. */
  void Dispose();

 private:
  friend RefPtrTest;
  friend class HeapTracerTest;
  friend class ObjectTrackerIntegration;
  friend class ObjectTrackerTest;

  /**
   * Used for testing when the objects being tracked exist on the stack.  This
   * removes all tracked objects.
   */
  void UnregisterAllObjects();

  /** @return Whether the given object is alive in JavaScript. */
  bool IsJsAlive(Traceable* object) const;

  /** @return The number of references to the given object. */
  uint32_t GetRefCount(Traceable* object) const;

  /** Used in tests to get all managed objects. */
  std::vector<const Traceable*> GetAllObjects() const;

  void DestroyObjects(const std::unordered_set<Traceable*>& to_delete,
                      std::unique_lock<Mutex>* lock);

  mutable Mutex mutex_;
  HeapTracer* tracer_;
  // A map of object pointer to ref count.
  std::unordered_map<Traceable*, uint32_t> objects_;
  std::unordered_map<Traceable*, uint64_t> last_alive_time_;
  std::unordered_set<Traceable*> to_delete_;
};

}  // namespace memory
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEMORY_OBJECT_TRACKER_H_
