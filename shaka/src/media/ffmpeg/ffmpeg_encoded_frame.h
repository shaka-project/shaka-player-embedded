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

#ifndef SHAKA_EMBEDDED_MEDIA_FFMPEG_FFMPEG_ENCODED_FRAME_H_
#define SHAKA_EMBEDDED_MEDIA_FFMPEG_FFMPEG_ENCODED_FRAME_H_

extern "C" {
#include <libavformat/avformat.h>
}

#include <memory>

#include "shaka/media/frames.h"

namespace shaka {
namespace media {
namespace ffmpeg {

/** This defines a single encoded media frame. */
class FFmpegEncodedFrame final : public EncodedFrame {
 public:
  ~FFmpegEncodedFrame() override;

  static FFmpegEncodedFrame* MakeFrame(AVPacket* pkt,
                                       std::shared_ptr<const StreamInfo> info,
                                       double timestamp_offset);

  size_t EstimateSize() const override;

 private:
  FFmpegEncodedFrame(AVPacket* pkt, double pts, double dts, double duration,
                     bool is_key_frame, std::shared_ptr<const StreamInfo> info,
                     std::shared_ptr<eme::FrameEncryptionInfo> encryption_info,
                     double timestamp_offset);

  AVPacket packet_;
};

}  // namespace ffmpeg
}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_FFMPEG_FFMPEG_ENCODED_FRAME_H_
