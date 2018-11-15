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

#include "src/media/base_frame.h"

namespace shaka {
namespace media {

BaseFrame::BaseFrame(double pts, double dts, double duration, bool is_key_frame)
    : pts(pts), dts(dts), duration(duration), is_key_frame(is_key_frame) {}
BaseFrame::~BaseFrame() {}

FrameType BaseFrame::frame_type() const {
  return FrameType::Unknown;
}

size_t BaseFrame::EstimateSize() const {
  return 0;
}

}  // namespace media
}  // namespace shaka
