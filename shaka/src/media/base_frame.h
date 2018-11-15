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

#ifndef SHAKA_EMBEDDED_MEDIA_BASE_FRAME_H_
#define SHAKA_EMBEDDED_MEDIA_BASE_FRAME_H_

#include "src/util/macros.h"

namespace shaka {
namespace media {

enum class FrameType {
  Unknown,
  FFmpegEncodedFrame,
  FFmpegDecodedFrame,
};


/**
 * This is the base class for a single media frame.  This contains common fields
 * between the encoded frames and the decoded frames.  These frames are created
 * by the Demuxer/Decoder and given to Stream.  Stream then maintains the
 * lifetime of the object.  See Stream for more info.
 */
class BaseFrame {
 public:
  BaseFrame(double pts, double dts, double duration, bool is_key_frame);
  virtual ~BaseFrame();

  NON_COPYABLE_OR_MOVABLE_TYPE(BaseFrame);

  /**
   * This gets the type of frame stored.  This is used to assert that the
   * correct frame types are being used.
   */
  virtual FrameType frame_type() const;

  /** @return An estimate of the number of bytes of memory this frame uses. */
  virtual size_t EstimateSize() const;

  const double pts;
  const double dts;
  const double duration;
  const bool is_key_frame;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_BASE_FRAME_H_
