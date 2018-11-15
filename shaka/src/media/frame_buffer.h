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

#ifndef SHAKA_EMBEDDED_MEDIA_FRAME_BUFFER_H_
#define SHAKA_EMBEDDED_MEDIA_FRAME_BUFFER_H_

#include <list>
#include <memory>
#include <vector>

#include "src/debug/mutex.h"
#include "src/media/base_frame.h"
#include "src/media/locked_frame_list.h"
#include "src/media/media_utils.h"
#include "src/util/macros.h"

namespace shaka {
namespace media {


/**
 * Defines a buffer of media frames.  This is stored as a series of contiguous
 * blocks of buffered ranges.
 *
 * This type is fully thread safe.  Any thread that gets frames MUST only use
 * the frame through the Guard instance to ensure that another thread doesn't
 * delete the frame while it is in use.
 */
class FrameBuffer {
 public:
  /**
   * The gap, in seconds, between frames that will still be considered part of
   * the same buffered range.  If two frames are further than this apart, then
   * they will be part of different buffered ranges.
   */
  static constexpr const double kMaxGapSize = 0.15;

  /**
   * Creates a new frame buffer.
   * @param order_by_dts True to order frames using DTS order; false to order by
   *   PTS.  When a frame is inserted, this determines where to insert the
   *   frame.
   */
  explicit FrameBuffer(bool order_by_dts);
  ~FrameBuffer();

  NON_COPYABLE_OR_MOVABLE_TYPE(FrameBuffer);

  /** @return An estimate of the number of bytes being used by these frames. */
  size_t EstimateSize() const;

  /** Adds a new frame to the buffer. */
  void AppendFrame(std::unique_ptr<const BaseFrame> frame);

  /**
   * Gets the ranges of buffered content in this buffer.  The times given are
   * based on the PTS time.  Because of frame reordering, the start of a range
   * may not have the same time as the first frame.
   */
  BufferedRanges GetBufferedRanges() const;

  /**
   * Gets the number of frames between the given times.  Both start and end are
   * exclusive bounds.
   * @return The number of frames.
   */
  int FramesBetween(double start_time, double end_time) const;


  /**
   * Gets the frame that is nearest to the given time.  This uses the start time
   * of the next frame and the end time of the frame before it; whichever is
   * closer will determine which frame to use.  It is undefined which will be
   * returned if the frames overlap.
   *
   * @returns The frame that is found, or nullptr if none are found.
   */
  LockedFrameList::Guard GetFrameNear(double time) const;
  /**
   * Gets the first frame that is after the frame with the given time.
   * Specifically, this will return the first frame whose start time is strictly
   * higher than the given time.
   * @returns The frame that is found, or nullptr if none are found.
   */
  LockedFrameList::Guard GetFrameAfter(double time) const;

  /**
   * Searches backward from the given time to return the first key frame.  If
   * there is a key frame at |time|, this will return that.
   * @returns The frame that is found, or nulptr if none are found.
   */
  LockedFrameList::Guard GetKeyFrameBefore(double time) const;

  /**
   * Removes the frames that start in the given range.  This will remove frames
   * past @a end until the next keyframe to mirror MSE requirements.
   *
   * Also to mirror MSE, this always uses PTS to determine which frames to
   * remove.  This will mean that some frames before |start| may be removed
   * because they depend on removed frames.
   *
   * If other threads are using frames from this buffer, this will block until
   * they are no longer being used.
   */
  void Remove(double start, double end);

 private:
  struct Range {
    Range();
    ~Range();
    NON_COPYABLE_TYPE(Range);

    std::list<std::unique_ptr<const BaseFrame>> frames;

    double start_pts;
    double end_pts;
  };

  const BaseFrame* GetFrameNear(double time, bool allow_before) const;
  void AssertRangesSorted() const;

  mutable LockedFrameList used_frames_;
  mutable Mutex mutex_;
  std::list<Range> buffered_ranges_;
  bool order_by_dts_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_FRAME_BUFFER_H_
