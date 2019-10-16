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

#ifndef SHAKA_EMBEDDED_MEDIA_FRAMES_H_
#define SHAKA_EMBEDDED_MEDIA_FRAMES_H_

#include <vector>

#include "../eme/implementation.h"
#include "../frame.h"  // TODO(modmaker): Merge this file into here.
#include "../macros.h"
#include "../variant.h"
#include "stream_info.h"

namespace shaka {
namespace media {

/**
 * Defines possible status results from media operations.
 * @ingroup media
 */
enum class MediaStatus : uint8_t {
  Success,

  /** A fatal error occurred and there is no way to recover. */
  FatalError,

  /**
   * Decryption failed since the required keys weren't found.  Decoding could
   * continue if the same frame was given again when the key is added.
   */
  KeyNotFound,
};

/**
 * Defines possible binary formats of raw audio data.
 *
 * For all formats, these are stored in native-endian byte order and assume the
 * volume has a range of [-1.0, 1.0].
 *
 * For planar data, each channel is stored in a different plane; for packed
 * formats, channels are stored interleaved.
 *
 * @ingroup media
 */
enum class SampleFormat : uint8_t {
  Unknown,

  /** Packed unsigned 8-bits. */
  PackedU8,
  /** Packed signed 16-bits. */
  PackedS16,
  /** Packed signed 32-bits. */
  PackedS32,
  /** Packed signed 64-bits. */
  PackedS64,
  /** Packed 32-bit floats. */
  PackedFloat,
  /** Packed 64-bits floats. */
  PackedDouble,

  /** Planar unsigned 8-bits. */
  PlanarU8,
  /** Planar signed 16-bits. */
  PlanarS16,
  /** Planar signed 32-bits. */
  PlanarS32,
  /** Planar signed 64-bits. */
  PlanarS64,
  /** Planar 32-bit floats. */
  PlanarFloat,
  /** Planar 64-bit floats. */
  PlanarDouble,

  /**
   * Apps can define custom sample formats and use any values above 128.  This
   * library doesn't care about the SampleFormat outside of the Decoder and the
   * AudioRenderer.
   */
  //{
  AppFormat1 = 128,
  AppFormat2 = 129,
  AppFormat3 = 130,
  AppFormat4 = 131,
  //}
};

/** @return Whether the given format is a planar format. */
SHAKA_EXPORT bool IsPlanarFormat(variant<PixelFormat, SampleFormat> format);

/**
 * @param format The format to check.
 * @param channels The number of audio channels; ignored for video formats.
 * @return The number of planes for the given format.
 */
SHAKA_EXPORT size_t GetPlaneCount(variant<PixelFormat, SampleFormat> format,
                                  size_t channels);


/**
 * Defines common info between encoded and decoded frames.
 *
 * @ingroup media
 */
class SHAKA_EXPORT BaseFrame {
 public:
  BaseFrame(double pts, double dts, double duration, bool is_key_frame);
  BaseFrame(const BaseFrame&) = delete;
  BaseFrame(BaseFrame&&) = delete;
  virtual ~BaseFrame();

  BaseFrame& operator=(const BaseFrame&) = delete;
  BaseFrame& operator=(BaseFrame&&) = delete;

  /** The absolute presentation timestamp, in seconds. */
  const double pts;

  /** The absolute decoding timestamp, in seconds. */
  const double dts;

  /** The duration of the frame, in seconds. */
  const double duration;

  /** Whether this frame is a keyframe. */
  const bool is_key_frame;

  /**
   * Estimates the size of this frame.  This can be used to restrict the number
   * of frames stored or to monitor/debug memory usage.  This should only be
   * used as an estimate and may not accurately measure the size.
   *
   * @return The estimated size of the frame, in bytes.
   */
  virtual size_t EstimateSize() const;
};

/**
 * Defines an encoded frame.  This can be used as-is, or can be subclassed to
 * support different frame types.
 *
 * This type doesn't support decrypting frames, but there is an internal
 * subclass that does.  The default Demuxer subclass produces frames that can be
 * decrypted, assuming EME support.  The Decrypt method is only needed if
 * using the DefaultMediaPlayer; a custom MediaPlayer instance can decrypt
 * how they want.
 *
 * @ingroup media
 */
class SHAKA_EXPORT EncodedFrame : public BaseFrame {
 public:
  EncodedFrame(double pts, double dts, double duration, bool is_key_frame,
               std::shared_ptr<const StreamInfo> stream, const uint8_t* data,
               size_t data_size, double timestamp_offset, bool is_encrypted);
  ~EncodedFrame() override;

  /**
   * Contains the info describing the current stream this belongs to.  If two
   * frames belong to the same stream, they must contain pointers to the same
   * StreamInfo object.
   */
  const std::shared_ptr<const StreamInfo> stream_info;

  /**
   * The buffer that contains the frame data.  This may contain encrypted data.
   */
  const uint8_t* const data;

  /** The number of bytes in @a data. */
  const size_t data_size;

  /**
   * The offset, in seconds, that the times in the frame should be adjusted when
   * decoding.
   */
  const double timestamp_offset;

  /** Whether the current frame is encrypted. */
  const bool is_encrypted;

  /**
   * Attempts to decrypt the frame's data into the given buffer.  This may not
   * be supported for some frame types or some EME implementations.  This is
   * only used by the DefaultMediaPlayer.
   */
  virtual MediaStatus Decrypt(const eme::Implementation* implementation,
                              uint8_t* output) const;


  size_t EstimateSize() const override;
};

/**
 * Defines a decoded frame.
 *
 * @ingroup media
 */
class SHAKA_EXPORT DecodedFrame : public BaseFrame {
 public:
  DecodedFrame(double pts, double dts, double duration,
               variant<PixelFormat, SampleFormat> format, uint32_t width,
               uint32_t height, uint32_t channel_count, uint32_t sample_rate,
               size_t sample_count, const std::vector<const uint8_t*>& data,
               const std::vector<size_t>& linesize);
  ~DecodedFrame() override;

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

  /**
   * If this is an audio frame, this is the number of samples (per channel) in
   * this frame.
   */
  const size_t sample_count;


  /**
   * The raw frame data for this frame.  The exact format of the data
   * depends on the format.  See PixelFormat/SampleFormat for the specific
   * formats.
   *
   * For hardware formats, this contains a single element pointing to the
   * hardware frame.
   *
   * For packed formats, this contains a single element containing the packed
   * data.
   *
   * For planar formats, this contains one element for each plane.
   */
  const std::vector<const uint8_t*> data;

  /**
   * Contains the line sizes.  Each element holds the line size value for the
   * associated plane in @a data.
   *
   * For audio, this holds the number of bytes in the plane; for video, this
   * holds the number of bytes in a row of the image.  Depending on the format,
   * there may or may not be the same number of rows as the @a height.
   */
  const std::vector<size_t> linesize;

  /** The format of this frame. */
  const variant<PixelFormat, SampleFormat> format;


  size_t EstimateSize() const override;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_FRAMES_H_
