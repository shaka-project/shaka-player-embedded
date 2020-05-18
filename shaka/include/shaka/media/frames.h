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

#include <iostream>
#include <vector>

#include "../eme/configuration.h"
#include "../eme/implementation.h"
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
 * Defines possible binary formats of raw texture data.
 *
 * @ingroup media
 */
enum class PixelFormat : uint8_t {
  Unknown,

  /**
   * Planar YUV 4:2:0, 12bpp.  This is FFmpeg's AV_PIX_FMT_YUV420P.
   *
   * The first plane holds the Y values for each pixel; each pixel has one byte.
   * The second and third planes hold U and V data respectively.  Each byte
   * in the row represents a 2x2 pixel region on the image.  This means that
   * the second and third planes have half as many bytes in each row.
   */
  YUV420P,

  /**
   * Planar YUV 4:2:0, 12bpp, using interleaved U/V components.  This is
   * FFmpeg's AV_PIX_FMT_NV12.
   *
   * The first plane holds Y values for each pixel, as a single byte.  The
   * second plane holds interleaved U/V components.  Each byte is alternating
   * U/V data where each pair represent a 2x2 pixel region on the image.
   */
  NV12,

  /**
   * Packed RGB 8:8:8, 24bpp.  This is FFmpeg's AV_PIX_FMT_RGB24.
   *
   * There is only one plane holding the data.  Each pixel is represented by
   * three bytes for R-G-B.
   */
  RGB24,


  /**
   * A VideoToolbox hardware encoded frame.  @a data[0] will contain a
   * CVPixelBufferRef object containing the texture.
   */
  VideoToolbox,

  /**
   * Apps can define custom pixel formats and use any values above 128.  This
   * library doesn't care about the PixelFormat outside of the Decoder and the
   * VideoRenderer.
   */
  //{
  AppFormat1 = 128,
  AppFormat2 = 129,
  AppFormat3 = 130,
  AppFormat4 = 131,
  //}
};

SHAKA_EXPORT std::ostream& operator<<(std::ostream& os, PixelFormat format);

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

SHAKA_EXPORT std::ostream& operator<<(std::ostream& os, SampleFormat format);

inline std::ostream& operator<<(std::ostream& os,
                                variant<PixelFormat, SampleFormat> format) {
  if (holds_alternative<PixelFormat>(format))
    return os << get<PixelFormat>(format);
  else
    return os << get<SampleFormat>(format);
}

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
  BaseFrame(std::shared_ptr<const StreamInfo> stream_info, double pts,
            double dts, double duration, bool is_key_frame);
  virtual ~BaseFrame();

  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(BaseFrame);

  /**
   * Contains the info describing the current stream this belongs to.  If two
   * frames belong to the same stream, they must contain pointers to the same
   * StreamInfo object.
   */
  const std::shared_ptr<const StreamInfo> stream_info;

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
 * @ingroup media
 */
class SHAKA_EXPORT EncodedFrame : public BaseFrame {
 public:
  EncodedFrame(std::shared_ptr<const StreamInfo> stream, double pts, double dts,
               double duration, bool is_key_frame, const uint8_t* data,
               size_t data_size, double timestamp_offset,
               std::shared_ptr<eme::FrameEncryptionInfo> encryption_info);
  ~EncodedFrame() override;

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

  /**
   * Contains info on how this frame is encrypted.  If this is nullptr, this
   * frame is clear.
   */
  const std::shared_ptr<eme::FrameEncryptionInfo> encryption_info;


  /**
   * Attempts to decrypt the frame's data into the given buffer.  This may not
   * be supported for some frame types or some EME implementations.  This is
   * only used by the DefaultMediaPlayer.
   */
  virtual MediaStatus Decrypt(const eme::Implementation* implementation,
                              uint8_t* dest) const;


  size_t EstimateSize() const override;
};

/**
 * Defines a decoded frame.
 *
 * @ingroup media
 */
class SHAKA_EXPORT DecodedFrame : public BaseFrame {
 public:
  DecodedFrame(std::shared_ptr<const StreamInfo> stream_info, double pts,
               double dts, double duration,
               variant<PixelFormat, SampleFormat> format, size_t sample_count,
               const std::vector<const uint8_t*>& data,
               const std::vector<size_t>& linesize);
  ~DecodedFrame() override;

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
