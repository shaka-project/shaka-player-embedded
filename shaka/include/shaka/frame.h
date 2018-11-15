// Copyright 2018 Google LLC
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

#ifndef SHAKA_EMBEDDED_FRAME_H_
#define SHAKA_EMBEDDED_FRAME_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "macros.h"

struct AVFrame;

namespace shaka {

namespace media {
class FrameDrawer;
}  // namespace media

/**
 * Defines possible binary formats of raw texture data.
 *
 * @ingroup player
 */
enum class PixelFormat {
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
   * A VideoToolbox hardware encoded frame.  @a data[3] will contain a
   * CVPixelBufferRef object containing the texture.
   */
  VIDEO_TOOLBOX,
};


/**
 * Represents a decoded frame holding pixel data.  This can represent either
 * a hardware texture from a hardware decoder or an array of pixel data that can
 * be copied to a texture.
 *
 * This also has a conversion function that can be used to convert to an
 * easier to use pixel format.
 *
 * @ingroup player
 */
class SHAKA_EXPORT Frame final {
 public:
  Frame();
  Frame(const Frame&) = delete;
  Frame(Frame&&);
  ~Frame();

  Frame& operator=(const Frame&) = delete;
  Frame& operator=(Frame&&);


  /** @return Whether this contains valid frame data. */
  bool valid() const;

  /** @return The pixel format of the frame. */
  PixelFormat pixel_format() const;

  /** @return The width of the frame in pixels. */
  uint32_t width() const;

  /** @return The height of the frame in pixels. */
  uint32_t height() const;

  /**
   * Gets the raw frame data for this frame.  The exact format of the data
   * depends on the pixel format.  See PixelFormat for the specific formats.
   *
   * In general, this returns a 4-element array of pointers to planar data.
   * Each pointer represents a separate plane.  For packed and hardware formats,
   * data[0] will contain the data.
   *
   * For non-hardware formats, each plane contains pixel data.  Each pixel is
   * represented by some number of bits going from left to right.  The
   * @a linesize function specifies how many bytes there are in each row of
   * the image.
   */
  const uint8_t* const* data() const;

  /**
   * Gets an array containing the line sizes.  Each element holds the line size
   * value for the associated plane in @a data.  The value represents the number
   * of bytes in a row of the image.
   */
  const int* linesize() const;


  /**
   * Tries to convert the frame data to the given pixel format.  If the
   * conversion fails, nothing is changed.  If the conversion succeeds, any
   * old data pointers are invalid.
   *
   * @param format The pixel format to convert to.
   * @return True on success; false on error.
   */
  bool ConvertTo(PixelFormat format);

 private:
  friend class media::FrameDrawer;
  Frame(AVFrame* frame);

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_FRAME_H_
