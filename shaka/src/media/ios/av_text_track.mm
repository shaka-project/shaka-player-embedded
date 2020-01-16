// Copyright 2020 Google LLC
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

#include "src/media/ios/av_text_track.h"

namespace shaka {
namespace media {
namespace ios {

AvTextTrack::AvTextTrack(AVPlayerItem *item, AVMediaSelectionGroup *group,
                         AVMediaSelectionOption *option)
    : AvMediaTrack(item, group, option),
      TextTrack(TextTrackKind::Subtitles, AvMediaTrack::label, AvMediaTrack::language,
                AvMediaTrack::id) {}

AvTextTrack::~AvTextTrack() {}

TextTrackMode AvTextTrack::mode() const {
  if (enabled())
    return TextTrackMode::Showing;
  else
    return TextTrackMode::Hidden;
}

void AvTextTrack::SetMode(TextTrackMode mode) {
  SetEnabled(mode == TextTrackMode::Showing);
}

}  // namespace ios
}  // namespace media
}  // namespace shaka
