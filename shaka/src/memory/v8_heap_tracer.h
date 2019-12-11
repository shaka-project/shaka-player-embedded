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

#ifndef SHAKA_EMBEDDED_MEMORY_V8_HEAP_TRACER_H_
#define SHAKA_EMBEDDED_MEMORY_V8_HEAP_TRACER_H_

#include <unordered_set>
#include <utility>
#include <vector>

#include "src/memory/heap_tracer.h"

namespace shaka {
namespace memory {

/**
 * This wraps the normal HeapTracer in an interface that can be used by V8
 * to track objects.  Methods defined here will be called by V8 when it decides
 * that a GC needs to be run.
 *
 * This explains how the V8 GC works and how we interact with it:
 *
 * There are two kinds of objects that are managed by different GCs, a
 * JavaScript object (v8::Value) which is handled by the V8 gc, and
 * BackingObject's which are handled by ObjectTracker.  When the V8 GC runs, we
 * need to tell it what objects we hold so it knows the V8 objects to delete.
 * That is the purpose of this class.
 *
 * When a V8 pass start, V8 will call TracePrologue.  Then it will traverse
 * its objects, marking alive objects.  Any object that looks like a wrapper
 * will be added to a list.  Once the traversal is done, it will call
 * RegisterV8References passing in the list.  Then it will call AdvanceTracing
 * to allow us to traverse our heap.  We should traverse our alive objects
 * and any wrapper objects given to us.  We should then (a) mark these object
 * as alive so we don't free them, and (b) tell V8 about any objects we hold.
 *
 * When we tell V8 about alive objects, it may need to do some more traversals,
 * which may in turn find more wrappers.  If this happens, it will call
 * RegisterV8References and AdvanceTracing again.
 *
 * At points between method calls, it is possible for JavaScript to run.
 * Because this runs on the event thread, it is not possible for JavaScript to
 * run while one of these methods are being called, but between it is possible.
 * V8 monitors all the objects and will ensure that any new objects will be
 * given to us.
 *
 * After V8 has traced every object TraceEpilogue is called.  We use this to
 * free any object that is not marked as alive.
 */
class V8HeapTracer : public v8::EmbedderHeapTracer, public HeapTracer {
 public:
  V8HeapTracer();
  ~V8HeapTracer() override;


  /** Called by V8 when a GC is aborted. */
  void AbortTracing() override;

  /** Called by V8 when a GC pass begins. */
  void TracePrologue() override;

  /** Called by V8 when a GC pass ends. */
  void TraceEpilogue() override;

  /**
   * Called by V8 when entering the final marking phase.  There will be no more
   * incremental marking calls.
   */
  void EnterFinalPause() override;

  /**
   * Called by V8 to tell us about wrapper objects.  The pair contains the
   * internal field values of the wrapper object.  We should store the values
   * and process them only in AdvanceTracing.
   */
  void RegisterV8References(
      const std::vector<std::pair<void*, void*>>& internal_fields) override;

  /**
   * Called by V8 to advance the GC run.  We should only take |deadline_ms|
   * time to complete, telling V8 whether there is more work to do.
   * @return True if there is more work to do, false if done.
   */
  bool AdvanceTracing(double deadline_ms, AdvanceTracingActions) override;

 private:
  std::unordered_set<const Traceable*> fields_;
};

}  // namespace memory
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEMORY_V8_HEAP_TRACER_H_
