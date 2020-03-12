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

#include "src/media/ios/av_media_track.h"

#include <atomic>

namespace shaka {
namespace media {
namespace ios {

namespace {

std::string GetId() {
  static std::atomic_int g_id_{1};
  const int id_ = g_id_.fetch_add(1, std::memory_order_relaxed);
  return "AvMediaTrack " + std::to_string(id_);
}

std::string MakeStr(NSString *str) {
  return str ? [str UTF8String] : "";
}


}  // namespace

AvMediaTrack::AvMediaTrack(AVPlayerItem *item, AVMediaSelectionGroup *group,
                           AVMediaSelectionOption *option)
    : MediaTrack(MediaTrackKind::Unknown, MakeStr(option.displayName),
                 MakeStr(option.extendedLanguageTag), GetId()),
      item_(item),
      group_(group),
      option_(option) {}

AvMediaTrack::~AvMediaTrack() {}

bool AvMediaTrack::enabled() const {
  return [item_.currentMediaSelection selectedMediaOptionInMediaSelectionGroup:group_] == option_;
}

void AvMediaTrack::SetEnabled(bool enabled) {
  if (enabled) {
    [item_ selectMediaOption:option_ inMediaSelectionGroup:group_];
  } else if (this->enabled()) {
    if (group_.allowsEmptySelection)
      [item_ selectMediaOption:nil inMediaSelectionGroup:group_];
    else
      [item_ selectMediaOption:group_.defaultOption inMediaSelectionGroup:group_];
  }
}

}  // namespace ios
}  // namespace media
}  // namespace shaka
