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

#ifndef SHAKA_EMBEDDED_MEDIA_LOCKED_FRAME_LIST_H_
#define SHAKA_EMBEDDED_MEDIA_LOCKED_FRAME_LIST_H_

#include <list>
#include <thread>
#include <unordered_set>

#include "src/debug/mutex.h"
#include "src/debug/thread_event.h"
#include "src/media/base_frame.h"
#include "src/util/macros.h"

namespace shaka {
namespace media {

/**
 * This tracks which frames are being used by other threads.  This also allows
 * for waiting until some set of them are no longer being used.  This handles
 * any internal synchronization needed.
 */
class LockedFrameList {
 public:
  LockedFrameList();
  ~LockedFrameList();

  NON_COPYABLE_OR_MOVABLE_TYPE(LockedFrameList);

  /**
   * A RAII type that is used to wrap and protect a single frame.  This object
   * MUST remain alive so long as the wrapped frame is being used.  Once this
   * object is destroyed, the contained frame can be destroyed.
   *
   * This is movable, but not copyable.  As such, this can be returned from
   * methods.  If this is moved, then the original instance no longer protects
   * (or contains) the frame, only the destination does.
   */
  class Guard {
   public:
    Guard();
    Guard(Guard&&);
    ~Guard();

    Guard& operator=(Guard&&);

    NON_COPYABLE_TYPE(Guard);

    operator bool() const {
      return frame_;
    }
    const BaseFrame* operator->() const {
      return frame_;
    }
    const BaseFrame& operator*() const {
      return *frame_;
    }

    bool operator==(const Guard& other) const {
      return frame_ == other.frame_;
    }
    bool operator!=(const Guard& other) const {
      return !(*this == other);
    }

    const BaseFrame* get() const {
      return frame_;
    }

   private:
    friend LockedFrameList;
    Guard(LockedFrameList* list, const BaseFrame* frame);

    void Destroy();

    LockedFrameList* list_;
    const BaseFrame* frame_;
  };

  /**
   * Protects the given frame from being deleted.  So long as the returned value
   * is kept alive, the frame can't be deleted (assuming the calling code uses
   * WaitToDeleteFrames).
   *
   * This may require external synchronization to avoid races between calling
   * this method and WaitToDeleteFrames; but once this call completes, no other
   * external synchronization is needed.
   */
  Guard GuardFrame(const BaseFrame* frame);

  /**
   * Blocks the current thread until all the given frames are unprotected.
   *
   * This may require external synchronization to avoid having the frame be
   * protected again once this returns.
   */
  void WaitToDeleteFrames(const std::unordered_set<const BaseFrame*>& frames);

 private:
  struct LockedFrame {
    const BaseFrame* frame;
#ifndef NDEBUG
    std::thread::id locked_thread;
#endif
  };

  void UnguardFrame(const BaseFrame* frame);

  Mutex mutex_;
  ThreadEvent<void> cond_;
  std::list<LockedFrame> frames_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_LOCKED_FRAME_LIST_H_
