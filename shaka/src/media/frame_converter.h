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

#ifndef SHAKA_EMBEDDED_MEDIA_FRAME_CONVERTER_H_
#define SHAKA_EMBEDDED_MEDIA_FRAME_CONVERTER_H_

extern "C" {
#ifdef HAS_SWSCALE
#  include <libswscale/swscale.h>
#endif
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
}

#include "src/util/macros.h"

namespace shaka {
namespace media {

/**
 * A class that converts a frame from one pixel format to another.
 * Acts as a wrapper over swscale.
 */
class FrameConverter {
 public:
  FrameConverter();
  ~FrameConverter();

  NON_COPYABLE_OR_MOVABLE_TYPE(FrameConverter);

  /**
   * Converts a frame to the given pixel format, using swscale.  The returned
   * data is only valid until this method is called again or when this object
   * is destroyed.
   *
   * @param frame The frame to be converted.
   * @param data A pointer that will be set to point to the new data of the
   *   converted frame if conversion is successful.
   * @param linesize A pointer that will be set to point to the new linesizes of
   *   the converted frame if conversion is successful.
   * @param desired_pixel_format The pixel format to convert to.
   * @return True if the conversion was successful, false otherwise.
   */
  bool ConvertFrame(const AVFrame* frame, const uint8_t* const** data,
                    const int** linesize, AVPixelFormat desired_pixel_format);

 private:
  AVFrame* cpu_frame_;
#ifdef HAS_SWSCALE
  SwsContext* sws_ctx_ = nullptr;
  uint8_t* convert_frame_data_[4] = {nullptr, nullptr, nullptr, nullptr};
  AVPixelFormat convert_pixel_format_ = AV_PIX_FMT_NONE;
  int convert_frame_linesize_[4] = {0, 0, 0, 0};
  int convert_frame_width_ = 0, convert_frame_height_ = 0;
#endif
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_FRAME_CONVERTER_H_
