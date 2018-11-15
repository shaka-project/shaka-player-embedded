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

#include "src/memory/v8_heap_tracer.h"

#include <glog/logging.h>

#include "src/mapping/backing_object.h"
#include "src/memory/object_tracker.h"
#include "src/util/clock.h"

namespace shaka {
namespace memory {

V8HeapTracer::V8HeapTracer(HeapTracer* heap_tracer,
                           ObjectTracker* object_tracker)
    : heap_tracer_(heap_tracer), object_tracker_(object_tracker) {}

V8HeapTracer::~V8HeapTracer() {}

void V8HeapTracer::AbortTracing() {
  VLOG(2) << "GC run aborted";
  heap_tracer_->ResetState();
  fields_.clear();
}

void V8HeapTracer::TracePrologue() {
  VLOG(2) << "GC run started";
  DCHECK(fields_.empty());
  heap_tracer_->BeginPass();
}

void V8HeapTracer::TraceEpilogue() {
  VLOG(2) << "GC run ended";
  CHECK(fields_.empty());
  object_tracker_->FreeDeadObjects(heap_tracer_->alive());
  heap_tracer_->ResetState();
}

void V8HeapTracer::EnterFinalPause() {}

void V8HeapTracer::RegisterV8References(
    const InternalFieldList& internal_fields) {
  VLOG(2) << "GC add " << internal_fields.size() << " objects";
  fields_.insert(fields_.end(), internal_fields.begin(), internal_fields.end());
}

bool V8HeapTracer::AdvanceTracing(double /* deadline_ms */,
                                  AdvanceTracingActions /* actions */) {
  VLOG(2) << "GC run step";
  util::Clock clock;
  const uint64_t start = clock.GetMonotonicTime();
  heap_tracer_->TraceCommon(object_tracker_->GetAliveObjects());
  for (auto& pair : fields_) {
    heap_tracer_->Trace(reinterpret_cast<BackingObject*>(pair.first));
  }

  VLOG(2) << "Tracing " << heap_tracer_->alive().size() << " objects took "
          << ((clock.GetMonotonicTime() - start) / 1000.0) << " seconds";
  fields_.clear();
  return false;
}

}  // namespace memory
}  // namespace shaka
