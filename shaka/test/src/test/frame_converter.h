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

#ifndef SHAKA_EMBEDDED_TEST_FRAME_CONVERTER_H_
#define SHAKA_EMBEDDED_TEST_FRAME_CONVERTER_H_

#include <memory>

#include "shaka/media/frames.h"
#include "src/util/macros.h"

struct SwsContext;

namespace shaka {

/**
 * A class that converts a frame from any pixel format to ARGB.
 * Acts as a wrapper over swscale.
 */
class FrameConverter {
 public:
  FrameConverter();
  ~FrameConverter();

  NON_COPYABLE_OR_MOVABLE_TYPE(FrameConverter);

  /**
   * Converts the given frame to ARGB.
   *
   * @param frame The input frame to convert.
   * @param data [OUT] Will store a pointer to the data.
   * @param size [OUT] Will store the number of bytes in @a data.
   */
  bool ConvertFrame(std::shared_ptr<media::DecodedFrame> frame,
                    const uint8_t** data, size_t* size);

 private:
  SwsContext* sws_ctx_ = nullptr;
  uint8_t* convert_frame_data_[4] = {nullptr, nullptr, nullptr, nullptr};
  int convert_frame_linesize_[4] = {0, 0, 0, 0};
  uint32_t convert_frame_width_ = 0, convert_frame_height_ = 0;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_TEST_FRAME_CONVERTER_H_
