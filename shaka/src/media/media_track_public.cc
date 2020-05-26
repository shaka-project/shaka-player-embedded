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

#include "shaka/media/media_track.h"

namespace shaka {
namespace media {

class MediaTrack::Impl {};

MediaTrack::MediaTrack(MediaTrackKind kind, const std::string& label,
                       const std::string& language, const std::string& id)
    : label(label), language(language), id(id), kind(kind), enabled_(false) {}

MediaTrack::~MediaTrack() {}

bool MediaTrack::enabled() const {
  return enabled_;
}

void MediaTrack::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

}  // namespace media
}  // namespace shaka
