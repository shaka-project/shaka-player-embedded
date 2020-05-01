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

void FitVideoToRegion(ShakaRect<uint32_t> frame, ShakaRect<uint32_t> bounds,
                      VideoFillMode mode, ShakaRect<uint32_t>* src,
                      ShakaRect<uint32_t>* dest) {
  const double width_factor = static_cast<double>(bounds.w) / frame.w;
  const double height_factor = static_cast<double>(bounds.h) / frame.h;
  const double max_factor = std::max(width_factor, height_factor);
  const double min_factor = std::min(width_factor, height_factor);

  switch (mode) {
    case VideoFillMode::Stretch:
      *src = frame;
      *dest = bounds;
      break;

    case VideoFillMode::Zoom:
      *dest = bounds;
      src->w = static_cast<uint32_t>(bounds.w / max_factor);
      src->h = static_cast<uint32_t>(bounds.h / max_factor);
      break;

    case VideoFillMode::MaintainRatio:
      *src = frame;
      dest->w = static_cast<uint32_t>(frame.w * min_factor);
      dest->h = static_cast<uint32_t>(frame.h * min_factor);
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

}  // namespace shaka
