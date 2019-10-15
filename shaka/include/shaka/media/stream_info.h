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

#include <string>
#include <type_traits>
#include <vector>

#include "../macros.h"

namespace shaka {
namespace media {

/**
 * Defines a rational number (i.e. a fraction) in a way to reduce the number
 * of rounding errors.  Some decoders can specify rational numbers separately,
 * so this avoids extra rounding errors when using floats.
 */
struct SHAKA_EXPORT Rational final {
  size_t numerator;
  size_t denominator;

  operator double() const {
    return static_cast<double>(numerator) / denominator;
  }

  bool operator==(const Rational& other) const {
    return numerator == other.numerator && denominator == other.denominator;
  }
  bool operator!=(const Rational& other) const {
    return !(*this == other);
  }
};
static_assert(std::is_pod<Rational>::value, "Rational should be POD");


/**
 * Defines information about a stream; this is used to initialize decoders.
 *
 * @ingroup media
 */
class SHAKA_EXPORT StreamInfo {
 public:
  StreamInfo(const std::string& mime, const std::string& codec, bool is_video,
             Rational time_scale, const std::vector<uint8_t>& extra_data);
  StreamInfo(const StreamInfo&) = delete;
  StreamInfo(StreamInfo&&) = delete;
  virtual ~StreamInfo();

  StreamInfo& operator=(const StreamInfo&) = delete;
  StreamInfo& operator=(StreamInfo&&) = delete;

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
  const Rational time_scale;

  /** Extra data used to initialize the decoder. */
  const std::vector<uint8_t> extra_data;

  /** True if this represents a video stream; false for audio streams. */
  const bool is_video;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_STREAM_INFO_H_
