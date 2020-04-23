// Copyright 2019 Google LLC
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

#ifndef SHAKA_EMBEDDED_MEDIA_STREAMS_H_
#define SHAKA_EMBEDDED_MEDIA_STREAMS_H_

#include <memory>
#include <type_traits>
#include <vector>

#include "../macros.h"
#include "frames.h"

namespace shaka {
namespace media {

enum class FrameLocation : uint8_t {
  /** Locates the first keyframe that starts before or at the given time. */
  KeyFrameBefore,
  /** Locates the frame that starts closest to the given time. */
  Near,
  /** Locates the frame that starts after the given time. */
  After,
};


/**
 * A POD type that holds a range of buffered frames in a stream.
 */
struct SHAKA_EXPORT BufferedRange {
  BufferedRange() = default;
  BufferedRange(double start, double end) : start(start), end(end) {}

  bool operator==(const BufferedRange& other) const {
    return start == other.start && end == other.end;
  }
  bool operator!=(const BufferedRange& other) const {
    return !(*this == other);
  }

  double start;
  double end;
};
static_assert(std::is_pod<BufferedRange>::value, "BufferedRange should be POD");


/**
 * Defines a base class for a stream containing frames. This represents a stream
 * of frames of a single type (i.e. video-only or audio-only); but this may
 * contain frames from different source streams (i.e. from adaptation).  Frames
 * are all within a single timeline and have been reordered/moved according to
 * MSE.
 *
 * This type is internally thread-safe.
 *
 * @ingroup media
 */
class SHAKA_EXPORT StreamBase {
 public:
  /**
   * The gap, in seconds, between frames that will still be considered part of
   * the same buffered range.  If two frames are further than this apart, then
   * they will be part of different buffered ranges.
   */
  static constexpr const double kMaxGapSize = 0.15;


  /**
   * Creates a new StreamBase that stores frames based on the given ordering.
   * This will order and query frames based on PTS or DTS.  So when you ask for
   * a time of N seconds, this will either use PTS or DTS to determine the
   * correct frame.  Some methods always use PTS to match requirements of MSE;
   * this means there may be differences between the times used in some methods.
   */
  explicit StreamBase(bool order_by_dts);
  ~StreamBase();

  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(StreamBase);

  /**
   * @param start The exclusive time to start from.
   * @param end The exclusive time to end at.
   * @return The number of frames that start within the given range.
   */
  size_t CountFramesBetween(double start, double end) const;

  /**
   * Returns the time ranges of the contiguously buffered ranges.  Each element
   * contains a start and end time of a range that has contiguous frames in it.
   * Each range should start with a keyframe and will be sorted.  This always
   * uses PTS to report the ranges.
   *
   * @return The time ranges of the buffered regions.
   */
  std::vector<BufferedRange> GetBufferedRanges() const;

  /**
   * Estimates the size of the stream by adding up all the stored frames.
   * @return The estimated size of the stream, in bytes.
   */
  size_t EstimateSize() const;


  /**
   * Removes any frames that start in the given range.  Since this type returns
   * a shared_ptr to the frames, they will not be destroyed until they are no
   * longer being used by the renderers or the app.
   *
   * This always uses PTS to determine what frames to remove.  This will keep
   * removing past the end until the next keyframe; all DemuxedFrame objects
   * are considered keyframes.
   *
   * @param start The inclusive time to start removing.
   * @param end The exclusive time to end removing.
   */
  void Remove(double start, double end);

  /** Removes all frames in the stream. */
  void Clear();


  /**
   * Prints debug info about the stream to stderr.
   * @param all_frames True to print times of all frames; false to only print a
   *   summary.
   */
  void DebugPrint(bool all_frames) const;

 protected:
  /**
   * Gets the frame that is associated with the given time.  How this searches
   * for frames depends on the second argument.  This may return nullptr if
   * there is no such frame.
   *
   * @param time The time (in seconds) to search for.
   * @param kind How to search for the frame.
   * @return The located frame, or nullptr if not found.
   */
  std::shared_ptr<BaseFrame> GetFrameInternal(double time,
                                              FrameLocation kind) const;

  /**
   * Adds a new frame to the stream.  This won't check for compatible streams
   * or for keyframe requirements; it is assumed the caller will only append
   * frames that can be played.  For example, the caller may need to discard
   * demuxed frames until they find a keyframe.
   *
   * If adding a frame with the same start time, this will replace the existing
   * frame; otherwise this will insert based on the start time, even if they
   * overlap.
   */
  void AddFrameInternal(std::shared_ptr<BaseFrame> frame);

 private:
  void AssertRangesSorted() const;

  class Impl;
  std::unique_ptr<Impl> impl_;
};


/**
 * Defines a stream that contains frames of the given type and has the given
 * frame ordering.
 *
 * This type is internally thread-safe.
 *
 * @ingroup media
 */
template <typename T, bool OrderByDts>
class SHAKA_EXPORT Stream : public StreamBase {
 public:
  Stream() : StreamBase(OrderByDts) {}

  /** @see StreamBase::AddFrameInternal */
  void AddFrame(std::shared_ptr<T> frame) {
    AddFrameInternal(frame);
  }

  /** @see StreamBase::GetFrameInternal */
  std::shared_ptr<T> GetFrame(
      double time, FrameLocation kind = FrameLocation::After) const {
    auto ret = GetFrameInternal(time, kind);
    return std::shared_ptr<T>(ret, static_cast<T*>(ret.get()));
  }
};


using ElementaryStream = Stream<EncodedFrame, true>;
using DecodedStream = Stream<DecodedFrame, false>;

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_STREAMS_H_
