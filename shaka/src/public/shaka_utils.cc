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

#include "shaka/utils.h"

namespace shaka {

ShakaRect FitVideoToWindow(int video_width, int video_height, int window_width,
                           int window_height, int window_x, int window_y) {
  const double width_scale = static_cast<double>(window_width) / video_width;
  const double height_scale = static_cast<double>(window_height) / video_height;
  int scaled_width, scaled_height;
  if (width_scale < height_scale) {
    scaled_width = window_width;
    scaled_height = static_cast<int>(video_height * width_scale);
  } else {
    scaled_width = static_cast<int>(video_width * height_scale);
    scaled_height = window_height;
  }
  return {.x = (window_width - scaled_width) / 2 + window_x,
          .w = scaled_width,
          .y = (window_height - scaled_height) / 2 + window_y,
          .h = scaled_height};
}

}  // namespace shaka
