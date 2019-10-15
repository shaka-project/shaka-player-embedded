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

#include <glog/logging.h>

#include <algorithm>

#include "shaka/utils.h"

namespace shaka {

void FitVideoToRegion(ShakaRect frame, ShakaRect bounds,
                      media::VideoFillMode mode, ShakaRect* src,
                      ShakaRect* dest) {
  const double width_factor = static_cast<double>(bounds.w) / frame.w;
  const double height_factor = static_cast<double>(bounds.h) / frame.h;
  const double max_factor = std::max(width_factor, height_factor);
  const double min_factor = std::min(width_factor, height_factor);

  switch (mode) {
    case media::VideoFillMode::Original:
      src->w = dest->w = std::min(frame.w, bounds.w);
      src->h = dest->h = std::min(frame.h, bounds.h);
      break;

    case media::VideoFillMode::Stretch:
      *src = frame;
      *dest = bounds;
      break;

    case media::VideoFillMode::Zoom:
      *dest = bounds;
      src->w = static_cast<int>(bounds.w / max_factor);
      src->h = static_cast<int>(bounds.h / max_factor);
      break;

    case media::VideoFillMode::MaintainRatio:
      *src = frame;
      dest->w = static_cast<int>(frame.w * min_factor);
      dest->h = static_cast<int>(frame.h * min_factor);
      break;

    default:
      LOG(FATAL) << "Unknown video fill mode: " << static_cast<int>(mode);
  }

  // Center the rect within its region.
  src->x = frame.x + (frame.w - src->w) / 2;
  src->y = frame.y + (frame.h - src->h) / 2;
  dest->x = bounds.x + (bounds.w - dest->w) / 2;
  dest->y = bounds.y + (bounds.h - dest->h) / 2;

  // Should always produce a sub-region of the input bounds.
  DCHECK_GE(src->x, frame.x);
  DCHECK_GE(src->y, frame.y);
  DCHECK_LE(src->x + src->w, frame.x + frame.w);
  DCHECK_LE(src->y + src->h, frame.y + frame.h);
  DCHECK_GE(dest->x, bounds.x);
  DCHECK_GE(dest->y, bounds.y);
  DCHECK_LE(dest->x + dest->w, bounds.x + bounds.w);
  DCHECK_LE(dest->y + dest->h, bounds.y + bounds.h);
}

ShakaRect FitVideoToWindow(int video_width, int video_height, int window_width,
                           int window_height, int window_x, int window_y) {
  ShakaRect src, dest;
  FitVideoToRegion({0, 0, video_width, video_height},
                   {window_x, window_y, window_width, window_height},
                   media::VideoFillMode::MaintainRatio, &src, &dest);
  return dest;
}

}  // namespace shaka
