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

#ifndef SHAKA_EMBEDDED_MEDIA_STREAM_INFO_H_
#define SHAKA_EMBEDDED_MEDIA_STREAM_INFO_H_

#include <memory>
#include <string>
#include <vector>

#include "../macros.h"
#include "../utils.h"

namespace shaka {
namespace media {

/**
 * Defines information about a stream; this is used to initialize decoders.
 *
 * @ingroup media
 */
class SHAKA_EXPORT StreamInfo {
 public:
  StreamInfo(const std::string& mime, const std::string& codec, bool is_video,
             Rational<uint32_t> time_scale,
             const std::vector<uint8_t>& extra_data, uint32_t width,
             uint32_t height, uint32_t channel_count, uint32_t sample_rate);
  virtual ~StreamInfo();

  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(StreamInfo);

  /**
   * The full MIME type of the input stream.  If the input is multiplexed, this
   * will contain multiple codecs.
   */
  const std::string mime_type;

  /**
   * The codec string this stream contains.  This is the name of the codec as
   * seen in @a mime_type.  This is a single codec, even for originally
   * multiplexed content.  If the original MIME type doesn't have a codec, this
   * returns an implementation-defined value for the codec.
   */
  const std::string codec;

  /**
   * The time-scale used in frame data.  In the encoded frame data, times are in
   * this timescale.  This doesn't apply to the "double" fields on the frame
   * object.
   */
  const Rational<uint32_t> time_scale;

  /** Extra data used to initialize the decoder. */
  const std::vector<uint8_t> extra_data;

  /** True if this represents a video stream; false for audio streams. */
  const bool is_video;

  /** If this is a video frame, this is the width, in pixels, of the frame. */
  const uint32_t width;

  /** If this is a video frame, this is the height, in pixels, of the frame. */
  const uint32_t height;

  /** If this is an audio frame, this is the number of channels. */
  const uint32_t channel_count;

  /**
   * If this is an audio frame, this is the sample rate, in samples per second
   * (Hz).
   */
  const uint32_t sample_rate;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_STREAM_INFO_H_
