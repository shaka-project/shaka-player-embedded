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

#ifndef SHAKA_EMBEDDED_MEDIA_FFMPEG_ENCODED_FRAME_H_
#define SHAKA_EMBEDDED_MEDIA_FFMPEG_ENCODED_FRAME_H_

extern "C" {
#include <libavformat/avformat.h>
}

#include <memory>

#include "src/media/base_frame.h"
#include "src/media/types.h"
#include "src/util/macros.h"

namespace shaka {

namespace eme {
class Implementation;
}  // namespace eme

namespace media {

/** This defines a single encoded media frame. */
class FFmpegEncodedFrame final : public BaseFrame {
 public:
  ~FFmpegEncodedFrame() override;

  static FFmpegEncodedFrame* MakeFrame(AVPacket* pkt, AVStream* stream,
                                       size_t stream_id,
                                       double timestamp_offset);

  NON_COPYABLE_OR_MOVABLE_TYPE(FFmpegEncodedFrame);

  FrameType frame_type() const override;
  size_t EstimateSize() const override;

  const AVPacket* raw_packet() const {
    return &packet_;
  }
  size_t stream_id() const {
    return stream_id_;
  }
  double timestamp_offset() const {
    return timestamp_offset_;
  }

  /** @return Whether this frame is encrypted. */
  bool is_encrypted() const;

  /**
   * Attempts to decrypt the frame into the given packet by the given CDM.  The
   * given packet should already been initialized with a buffer large enough to
   * hold the current frame.
   */
  Status Decrypt(eme::Implementation* cdm, AVPacket* dest_packet) const;

 private:
  FFmpegEncodedFrame(AVPacket* pkt, size_t stream_id, double offset, double pts,
                     double dts, double duration, bool is_key_frame);

  AVPacket packet_;
  const size_t stream_id_;
  const double timestamp_offset_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_FFMPEG_ENCODED_FRAME_H_
