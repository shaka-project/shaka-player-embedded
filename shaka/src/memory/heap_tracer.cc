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

#include "src/memory/heap_tracer.h"

#include <utility>

#include "src/core/js_manager_impl.h"
#include "src/mapping/backing_object.h"
#include "src/memory/object_tracker.h"
#include "src/util/utils.h"

namespace shaka {
namespace memory {

bool Traceable::IsRootedAlive() const {
  return false;
}
bool Traceable::IsShortLived() const {
  return false;
}


HeapTracer::HeapTracer() : mutex_("HeapTracer") {}
HeapTracer::~HeapTracer() {}

void HeapTracer::ForceAlive(const Traceable* ptr) {
  std::unique_lock<Mutex> lock(mutex_);
  pending_.insert(ptr);
}

void HeapTracer::Trace(const Traceable* ptr) {
  std::unique_lock<Mutex> lock(mutex_);
  pending_.insert(ptr);
}

void HeapTracer::BeginPass() {
  ResetState();
}

void HeapTracer::TraceCommon(
    const std::unordered_set<const Traceable*>& ref_alive) {
  {
    std::unique_lock<Mutex> lock(mutex_);
    pending_.insert(ref_alive.begin(), ref_alive.end());
  }

  while (true) {
    std::unordered_set<const Traceable*> to_trace;

    {
      std::unique_lock<Mutex> lock(mutex_);
      to_trace = std::move(pending_);

      // We need to be careful about circular dependencies.  Only traverse if we
      // have not seen it before.
      for (auto it = to_trace.begin(); it != to_trace.end();) {
        if (*it && alive_.count(*it) == 0) {
          alive_.insert(*it);
          ++it;
        } else {
          it = to_trace.erase(it);
        }
      }
    }

    if (to_trace.empty())
      break;
    for (const Traceable* ptr : to_trace) {
      ptr->Trace(this);
    }
  }
}

void HeapTracer::ResetState() {
  std::unique_lock<Mutex> lock(mutex_);

  alive_.clear();
  pending_.clear();
}

}  // namespace memory
}  // namespace shaka
