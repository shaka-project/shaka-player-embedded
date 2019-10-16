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

#include "src/media/frame_drawer.h"

#include "shaka/media/frames.h"
#include "src/media/ffmpeg_decoded_frame.h"

namespace shaka {
namespace media {

FrameDrawer::FrameDrawer() {}
FrameDrawer::~FrameDrawer() {}

Frame FrameDrawer::DrawFrame(const BaseFrame* frame) {
  auto* cast_frame = static_cast<const FFmpegDecodedFrame*>(frame);
  return Frame(cast_frame->raw_frame());
}

}  // namespace media
}  // namespace shaka
