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

#ifndef SHAKA_EMBEDDED_MEDIA_FFMPEG_FFMPEG_DECODER_H_
#define SHAKA_EMBEDDED_MEDIA_FFMPEG_FFMPEG_DECODER_H_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <memory>
#include <string>
#include <vector>

#include "shaka/media/decoder.h"
#include "shaka/media/frames.h"
#include "shaka/media/stream_info.h"
#include "src/debug/mutex.h"

namespace shaka {
namespace media {
namespace ffmpeg {

/**
 * An implementation of the Decoder type that uses FFmpeg to decode frames.
 * This produces FFmpegDecodedFrame objects.
 */
class FFmpegDecoder : public Decoder {
 public:
  FFmpegDecoder();
  ~FFmpegDecoder() override;

  MediaCapabilitiesInfo DecodingInfo(
      const MediaDecodingConfiguration& config) const override;

  void ResetDecoder() override;

  MediaStatus Decode(
      std::shared_ptr<EncodedFrame> input, const eme::Implementation* eme,
      std::vector<std::shared_ptr<DecodedFrame>>* frames,
      std::string* extra_info) override;

 private:
#ifdef ENABLE_HARDWARE_DECODE
  static AVPixelFormat GetPixelFormat(AVCodecContext* ctx,
                                      const AVPixelFormat* formats);
#endif

  bool InitializeDecoder(std::shared_ptr<const StreamInfo> info,
                         bool allow_hardware,
                         std::string* extra_info);
  bool ReadFromDecoder(std::shared_ptr<const StreamInfo> stream_info,
                       std::shared_ptr<EncodedFrame> input,
                       std::vector<std::shared_ptr<DecodedFrame>>* decoded,
                       std::string* extra_info);

  Mutex mutex_;
  const std::string codec_;

  AVCodecContext* decoder_ctx_;
  AVFrame* received_frame_;
#ifdef ENABLE_HARDWARE_DECODE
  AVBufferRef* hw_device_ctx_;
  AVPixelFormat hw_pix_fmt_;
#endif
  double prev_timestamp_offset_;
  // The stream the decoder is currently configured to use.
  std::shared_ptr<const StreamInfo> decoder_stream_info_;
};

}  // namespace ffmpeg
}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_FFMPEG_FFMPEG_DECODER_H_
