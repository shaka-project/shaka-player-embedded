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

#ifndef SHAKA_EMBEDDED_MAPPING_BACKING_OBJECT_H_
#define SHAKA_EMBEDDED_MAPPING_BACKING_OBJECT_H_

#include <glog/logging.h>

#include <string>

#include "src/mapping/js_wrappers.h"
#include "src/mapping/weak_js_ptr.h"
#include "src/memory/heap_tracer.h"

namespace shaka {

namespace memory {
class HeapTracer;
class ObjectTracker;
}  // namespace memory

namespace js {
class Debug;
}  // namespace js

class BackingObjectFactoryBase;

#define DECLARE_TYPE_INFO(type) \
 public:                        \
  static std::string name() {   \
    return #type;               \
  }                             \
                                \
 protected:                     \
  ~type() override;             \
                                \
 public:                        \
  ::shaka::BackingObjectFactoryBase* factory() const override

/**
 * A base type for objects exposed to JavaScript.  This is the backing type for
 * a JavaScript object.  This contains a weak pointer to the object that it
 * backs.
 */
class BackingObject : public memory::Traceable {
 public:
  BackingObject();

  /** The number of internal fields in a wrapper object. */
  static constexpr const size_t kInternalFieldCount = 2;

  // In WebKit, it says that MSVC requires delete[] for exported classes.
  // https://goo.gl/eXrvjn
  // Disable new[] and delete[] since we use 'delete' in the ObjectTracker to
  // delete objects and that does not work with arrays.
  static void* operator new[](size_t size) = delete;
  static void operator delete[](void*) = delete;

  void Trace(memory::HeapTracer* tracer) const override;
  bool IsRootedAlive() const override;

  /** @return The name of the type. */
  std::string name() const;

  /** @return the factory that created this object. */
  virtual BackingObjectFactoryBase* factory() const = 0;

  /** @return Whether this object derives from the type with the given name. */
  bool DerivedFrom(const std::string& base);

  /**
   * Gets the JavaScript object that represents this instance.  It is only
   * valid to call this method on the Event thread.
   */
  ReturnVal<JsValue> JsThis() const;

  /** Sets the JavaScript instance that represents this object. */
  void SetJsThis(Handle<JsObject> this_);

 protected:
  friend class memory::ObjectTracker;
  ~BackingObject() override;

 private:
  WeakJsPtr<JsObject> js_this_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_BACKING_OBJECT_H_
