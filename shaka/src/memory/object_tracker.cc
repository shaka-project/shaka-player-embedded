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

#include "src/memory/object_tracker.h"

#include "src/mapping/backing_object.h"
#include "src/memory/heap_tracer.h"
#include "src/util/clock.h"
#include "src/util/utils.h"

namespace shaka {
namespace memory {

void ObjectTracker::RegisterObject(Traceable* object) {
  std::unique_lock<Mutex> lock(mutex_);
  DCHECK(objects_.count(object) == 0 || to_delete_.count(object) == 1);
  objects_.emplace(object, 0u);
  to_delete_.erase(object);

  if (object->IsShortLived())
    last_alive_time_.emplace(object, util::Clock::Instance.GetMonotonicTime());
}

void ObjectTracker::ForceAlive(const Traceable* ptr) {
  std::unique_lock<Mutex> lock(mutex_);
  tracer_->ForceAlive(ptr);
}

void ObjectTracker::AddRef(const Traceable* object) {
  if (object) {
    std::unique_lock<Mutex> lock(mutex_);
    auto* key = const_cast<Traceable*>(object);  // NOLINT
    DCHECK_EQ(objects_.count(key), 1u);
    objects_[key]++;

    tracer_->ForceAlive(object);
  }
}

void ObjectTracker::RemoveRef(const Traceable* object) {
  if (object) {
    std::unique_lock<Mutex> lock(mutex_);
    auto* key = const_cast<Traceable*>(object);  // NOLINT
    DCHECK_EQ(objects_.count(key), 1u);
    CHECK_GT(objects_[key], 0u);
    objects_[key]--;

    // Don't use IsShortLived() here since |object| may be an invalid pointer.
    // During Dispose(), objects may be destroyed with existing references to
    // them.  This means that |object| may be an invalid pointer.
    if (last_alive_time_.count(key) > 0)
      last_alive_time_[key] = util::Clock::Instance.GetMonotonicTime();
  }
}

std::unordered_set<const Traceable*> ObjectTracker::GetAliveObjects() const {
  std::unique_lock<Mutex> lock(mutex_);
  std::unordered_set<const Traceable*> ret;
  ret.reserve(objects_.size());
  for (auto& pair : objects_) {
    if (pair.second != 0 || IsJsAlive(pair.first))
      ret.insert(pair.first);
  }
  return ret;
}

void ObjectTracker::FreeDeadObjects(
    const std::unordered_set<const Traceable*>& alive) {
  std::unique_lock<Mutex> lock(mutex_);
  std::unordered_set<Traceable*> to_delete;
  to_delete.reserve(objects_.size());
  for (auto pair : objects_) {
    // |alive| also contains objects that have a non-zero ref count.  But we
    // need to check against our ref count also to ensure new objects that
    // are created while the GC is running are not deleted.
    if (pair.second == 0u && alive.count(pair.first) == 0 &&
        !IsJsAlive(pair.first)) {
      to_delete.insert(pair.first);
    }
  }
  to_delete_ = to_delete;

  DestroyObjects(to_delete, &lock);
}

ObjectTracker::ObjectTracker()
    : tracer_(new HeapTracer()), mutex_("ObjectTracker") {}

ObjectTracker::~ObjectTracker() {
  CHECK(objects_.empty());
}

void ObjectTracker::UnregisterAllObjects() {
  std::unique_lock<Mutex> lock(mutex_);
  last_alive_time_.clear();
  objects_.clear();
}

bool ObjectTracker::IsJsAlive(Traceable* object) const {
  const uint64_t now = util::Clock::Instance.GetMonotonicTime();
  if (object->IsShortLived()) {
    if (last_alive_time_.count(object) == 0)
      return false;

    return last_alive_time_.at(object) + Traceable::kShortLiveDurationMs > now;
  }
  return object->IsRootedAlive();
}

uint32_t ObjectTracker::GetRefCount(Traceable* object) const {
  std::unique_lock<Mutex> lock(mutex_);
  DCHECK_EQ(1u, objects_.count(object));
  return objects_.at(object);
}

std::vector<const Traceable*> ObjectTracker::GetAllObjects() const {
  std::unique_lock<Mutex> lock(mutex_);
  std::vector<const Traceable*> ret;
  ret.reserve(objects_.size());
  for (auto& pair : objects_)
    ret.push_back(pair.first);
  return ret;
}

void ObjectTracker::Dispose() {
  std::unique_lock<Mutex> lock(mutex_);
  while (!objects_.empty()) {
    std::unordered_set<Traceable*> to_delete;
    for (auto& pair : objects_)
      to_delete.insert(pair.first);
    to_delete_ = to_delete;

    DestroyObjects(to_delete, &lock);
  }
}

void ObjectTracker::DestroyObjects(
    const std::unordered_set<Traceable*>& to_delete,
    std::unique_lock<Mutex>* lock) {
  DCHECK(lock->owns_lock());
  {
    util::Unlocker<Mutex> unlock(lock);
    // Don't hold lock so destructor can call AddRef.
    for (Traceable* item : to_delete)
      delete item;
  }
  VLOG(1) << "Deleted " << to_delete.size() << " object(s).";

  // Don't remove elements from |objects_| until after the destructor so the
  // destructor can call AddRef.
  for (auto it = objects_.begin(); it != objects_.end();) {
    if (to_delete_.count(it->first) > 0) {
      last_alive_time_.erase(it->first);
      it = objects_.erase(it);
    } else {
      it++;
    }
  }
}

}  // namespace memory
}  // namespace shaka
