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

#include "src/media/stream.h"

#include <algorithm>

namespace shaka {
namespace media {

Stream::Stream()
    : demuxed_frames_(/* order_by_dts */ true),
      decoded_frames_(/* order_by_dts */ false) {}

Stream::~Stream() {}

double Stream::DecodedAheadOf(double time) const {
  for (auto& range : decoded_frames_.GetBufferedRanges()) {
    if (range.end > time) {
      if (range.start < time + FrameBuffer::kMaxGapSize) {
        return range.end - std::max(time, range.start);
      }

      // The ranges are sorted, so avoid looking at the remaining elements.
      break;
    }
  }
  return 0;
}

}  // namespace media
}  // namespace shaka
