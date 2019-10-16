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

#include "shaka/media/frames.h"

namespace shaka {
namespace media {

/** This defines a single encoded media frame. */
class FFmpegEncodedFrame final : public EncodedFrame {
 public:
  ~FFmpegEncodedFrame() override;

  static FFmpegEncodedFrame* MakeFrame(AVPacket* pkt, AVStream* stream,
                                       size_t stream_id,
                                       double timestamp_offset);

  const AVPacket* raw_packet() const {
    return &packet_;
  }
  size_t stream_id() const {
    // TODO(modmaker): Change to use stream_info.
    return stream_id_;
  }

  MediaStatus Decrypt(const eme::Implementation* cdm,
                      uint8_t* data) const override;
  size_t EstimateSize() const override;

 private:
  FFmpegEncodedFrame(AVPacket* pkt, double pts, double dts, double duration,
                     bool is_key_frame, size_t stream_id,
                     double timestamp_offset);

  AVPacket packet_;
  const size_t stream_id_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_FFMPEG_ENCODED_FRAME_H_
