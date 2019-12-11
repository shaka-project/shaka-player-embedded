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

V8HeapTracer::V8HeapTracer() {}

V8HeapTracer::~V8HeapTracer() {}

void V8HeapTracer::AbortTracing() {
  VLOG(2) << "GC run aborted";
  ResetState();
  fields_.clear();
}

void V8HeapTracer::TracePrologue() {
  VLOG(2) << "GC run started";
  fields_ = ObjectTracker::Instance()->GetAliveObjects();
  BeginPass();
}

void V8HeapTracer::TraceEpilogue() {
  VLOG(2) << "GC run ended";
  CHECK(fields_.empty());
  ObjectTracker::Instance()->FreeDeadObjects(alive());
  ResetState();
}

void V8HeapTracer::EnterFinalPause() {}

void V8HeapTracer::RegisterV8References(
    const std::vector<std::pair<void*, void*>>& internal_fields) {
  VLOG(2) << "GC add " << internal_fields.size() << " objects";
  fields_.reserve(fields_.size() + internal_fields.size());
  for (const auto& pair : internal_fields)
    fields_.insert(reinterpret_cast<Traceable*>(pair.first));
}

bool V8HeapTracer::AdvanceTracing(double /* deadline_ms */,
                                  AdvanceTracingActions /* actions */) {
  VLOG(2) << "GC run step";
  util::Clock clock;
  const uint64_t start = clock.GetMonotonicTime();
  TraceAll(fields_);

  VLOG(2) << "Tracing " << fields_.size() << " objects took "
          << ((clock.GetMonotonicTime() - start) / 1000.0) << " seconds";
  fields_.clear();
  return false;
}

}  // namespace memory
}  // namespace shaka
