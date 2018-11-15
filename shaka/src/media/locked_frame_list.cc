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

#include "src/media/locked_frame_list.h"

namespace shaka {
namespace media {

LockedFrameList::Guard::Guard() : list_(nullptr), frame_(nullptr) {}

LockedFrameList::Guard::Guard(LockedFrameList* list, const BaseFrame* frame)
    : list_(list), frame_(frame) {}

LockedFrameList::Guard::Guard(Guard&& other)
    : list_(other.list_), frame_(other.frame_) {
  other.list_ = nullptr;
  other.frame_ = nullptr;
}

LockedFrameList::Guard::~Guard() {
  Destroy();
}

LockedFrameList::Guard& LockedFrameList::Guard::operator=(Guard&& other) {
  Destroy();
  list_ = other.list_;
  frame_ = other.frame_;
  other.list_ = nullptr;
  other.frame_ = nullptr;
  return *this;
}

void LockedFrameList::Guard::Destroy() {
  if (list_) {
    list_->UnguardFrame(frame_);
    list_ = nullptr;
    frame_ = nullptr;
  }
}

LockedFrameList::LockedFrameList()
    : mutex_("LockedFrameList"), cond_("Frame delete") {}

LockedFrameList::~LockedFrameList() {
  DCHECK(frames_.empty());
}

LockedFrameList::Guard LockedFrameList::GuardFrame(const BaseFrame* frame) {
  if (!frame)
    return Guard();

  std::unique_lock<Mutex> lock(mutex_);
#ifndef NDEBUG
  frames_.push_back({frame, std::this_thread::get_id()});
#else
  frames_.push_back({frame});
#endif
  return Guard(this, frame);
}

void LockedFrameList::WaitToDeleteFrames(
    const std::unordered_set<const BaseFrame*>& frames) {
  std::unique_lock<Mutex> lock(mutex_);

#ifndef NDEBUG
  for (auto& locked_frame : frames_) {
    if (frames.count(locked_frame.frame) > 0)
      CHECK_NE(locked_frame.locked_thread, std::this_thread::get_id())
          << "Cannot delete a frame in used by this thread.";
  }
#endif

  auto has_locked = [&]() {
    for (auto& locked_frame : frames_) {
      if (frames.count(locked_frame.frame) > 0)
        return true;
    }
    return false;
  };
  while (has_locked()) {
    cond_.ResetAndWaitWhileUnlocked(lock);
  }
}

void LockedFrameList::UnguardFrame(const BaseFrame* frame) {
  std::unique_lock<Mutex> lock(mutex_);
  for (auto it = frames_.begin(); it != frames_.end(); it++) {
    if (it->frame == frame) {
      frames_.erase(it);
      cond_.SignalAllIfNotSet();
      return;
    }
  }
}

}  // namespace media
}  // namespace shaka
