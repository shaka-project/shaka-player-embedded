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
                      Rational<uint32_t> sample_aspect_ratio,
                      VideoFillMode mode, ShakaRect<uint32_t>* src,
                      ShakaRect<uint32_t>* dest) {
  if (!sample_aspect_ratio)
    sample_aspect_ratio = Rational<uint32_t>{1, 1};

  switch (mode) {
    // Stretch video to fill screen.
    case VideoFillMode::Stretch:
      *src = frame;
      *dest = bounds;
      break;

    // Crop video to maintain aspect ratio while filling the screen.
    case VideoFillMode::Zoom: {
      *dest = bounds;
      const Rational<uint32_t> bounds_ratio{bounds.w, bounds.h};
      // We either use the whole width or the whole height; the other dimension
      // gets cropped based on the aspect ratio.
      // Convert number of pixels to image size, then use display aspect ratio.
      src->w = std::min(
          frame.w, (frame.h / sample_aspect_ratio * bounds_ratio).truncate());
      src->h = std::min(
          frame.h, (frame.w * sample_aspect_ratio / bounds_ratio).truncate());
      break;
    }

    // Add black boxes around video to maintain aspect ratio.
    case VideoFillMode::MaintainRatio: {
      *src = frame;
      const Rational<uint32_t> aspect_ratio =
          Rational<uint32_t>{frame.w, frame.h} * sample_aspect_ratio;
      // Get the width based on the height and the display aspect ratio.
      dest->w = std::min(bounds.w, (bounds.h * aspect_ratio).truncate());
      dest->h = std::min(bounds.h, (bounds.w / aspect_ratio).truncate());
      DCHECK(dest->w == bounds.w || dest->h == bounds.h);
      break;
    }

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
