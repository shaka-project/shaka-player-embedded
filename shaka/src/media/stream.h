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

#ifndef SHAKA_EMBEDDED_MEDIA_STREAM_H_
#define SHAKA_EMBEDDED_MEDIA_STREAM_H_

#include <vector>

#include "shaka/media/frames.h"
#include "shaka/media/streams.h"
#include "src/util/macros.h"

namespace shaka {
namespace media {

/**
 * This maintains two buffers of media frames.  One buffer (the demuxed buffer)
 * contains demuxed, encoded media frames.  The other (the decoded buffer)
 * contains decoded, full media frames.
 *
 * The demuxed buffer defines the buffered ranges in MSE.  This buffer is the
 * larger of the two and the data will likely live longer.  The data will only
 * be freed when we run out of memory or if JavaScript tells us to through a
 * call to remove().
 *
 * The decoded buffered is smaller and only contains frames slightly ahead of
 * the playhead.  When the playhead passes a frame, it is dropped.
 *
 * This class handles reordering frames as needed and controls the lifetime of
 * the frames.  Other classes will insert data into this object.
 *
 * This class is fully thread-safe.  It is fine to append frames from background
 * threads and remove them from others.
 */
class Stream {
 public:
  Stream();
  ~Stream();

  NON_COPYABLE_OR_MOVABLE_TYPE(Stream);

  /** @return The amount of time decoded ahead of the given time. */
  double DecodedAheadOf(double time) const;

  /** @returns The buffered ranges for the Stream. */
  std::vector<BufferedRange> GetBufferedRanges() const {
    return demuxed_frames_.GetBufferedRanges();
  }

  StreamNew<BaseFrame, true>* GetDemuxedFrames() {
    return &demuxed_frames_;
  }
  const StreamNew<BaseFrame, true>* GetDemuxedFrames() const {
    return &demuxed_frames_;
  }
  StreamNew<BaseFrame, false>* GetDecodedFrames() {
    return &decoded_frames_;
  }
  const StreamNew<BaseFrame, false>* GetDecodedFrames() const {
    return &decoded_frames_;
  }

 private:
  // TODO(modmaker): Use ElementaryStream/DecodedStream.
  StreamNew<BaseFrame, true> demuxed_frames_;
  StreamNew<BaseFrame, false> decoded_frames_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_STREAM_H_
