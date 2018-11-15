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

#ifndef SHAKA_EMBEDDED_MEDIA_FFMPEG_DECODED_FRAME_H_
#define SHAKA_EMBEDDED_MEDIA_FFMPEG_DECODED_FRAME_H_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <memory>

#include "src/media/base_frame.h"
#include "src/util/macros.h"

namespace shaka {
namespace media {

/** This defines a single decoded media frame. */
class FFmpegDecodedFrame final : public BaseFrame {
 public:
  ~FFmpegDecodedFrame() override;

  static FFmpegDecodedFrame* CreateFrame(AVFrame* frame, double time,
                                         double duration);

  NON_COPYABLE_OR_MOVABLE_TYPE(FFmpegDecodedFrame);

  FrameType frame_type() const override;
  size_t EstimateSize() const override;

  /** @return The width of the frame in pixels, if this is video. */
  int width() const {
    return frame_->width;
  }

  /** @return The height of the frame in pixels, if this is video. */
  int height() const {
    return frame_->height;
  }

  /** @return The pixel format of the frame, if this is video. */
  AVPixelFormat pixel_format() const {
    return static_cast<AVPixelFormat>(frame_->format);
  }

  /** @return The sample format of the frame, if this is audio. */
  AVSampleFormat sample_format() const {
    return static_cast<AVSampleFormat>(frame_->format);
  }

  AVFrame* raw_frame() const {
    return frame_;
  }


  /**
   * Gets the raw frame data for this frame.  The exact format of the data and
   * the size of the data depends on the pixel/sample format.
   *
   * For hardware-accelerated formats, the data() contains pointers to some
   * internal structures to track hardware buffers.
   *
   * For audio, each element contains an audio channel.  Each channel contains
   * the samples for that channel in rendering order.  The size of the buffer is
   * specified in linesize().
   *
   * For video, it depends on packed vs planar formats.  In either case each
   * element contains pixel data.  It is stored as an array of pixels, left to
   * right, top to bottom.  linesize() specifies the length of a row of pixels,
   * in bytes.  The number of rows depends on the pixel format.
   *
   * For packed video formats, there is only one element that contains all the
   * pixel data.  The number of rows is equal to the height in pixels.
   *
   * For planar video formats, each element specifies a plane.  For example,
   * planar YUV will have three planes: Y, U, and V.  The number of rows depends
   * on the pixel format.
   */
  uint8_t** data() const;

  /**
   * Gets an array of line sizes for the frame.  The exact interpretation and
   * number of elements depends on the pixel/sample format.  Each element of
   * this is associated with an element in data().
   *
   * @see data()
   */
  int* linesize() const;

 private:
  FFmpegDecodedFrame(AVFrame* frame, double pts, double dts, double duration);

  AVFrame* frame_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_FFMPEG_DECODED_FRAME_H_
