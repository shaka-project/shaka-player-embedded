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

#include "src/js/mse/media_track.h"

namespace shaka {
namespace js {
namespace mse {

MediaTrack::MediaTrack(std::shared_ptr<media::MediaTrack> track)
    : track_(track) {}

MediaTrack::~MediaTrack() {}

const std::string& MediaTrack::label() const {
  return track_->label;
}

const std::string& MediaTrack::language() const {
  return track_->language;
}

const std::string& MediaTrack::id() const {
  return track_->id;
}

media::MediaTrackKind MediaTrack::kind() const {
  return track_->kind;
}

bool MediaTrack::enabled() const {
  return track_->enabled();
}

void MediaTrack::SetEnabled(bool enabled) {
  track_->SetEnabled(enabled);
}


// \cond Doxygen_Skip
AudioTrack::~AudioTrack() {}
VideoTrack::~VideoTrack() {}
// \endcond Doxygen_Skip


AudioTrackFactory::AudioTrackFactory() {
  AddGenericProperty<AudioTrack>("kind", &MediaTrack::kind);
  AddGenericProperty<AudioTrack>("label", &MediaTrack::label);
  AddGenericProperty<AudioTrack>("language", &MediaTrack::language);
  AddGenericProperty<AudioTrack>("id", &MediaTrack::id);
  AddGenericProperty<AudioTrack>("enabled", &MediaTrack::enabled,
                                 &MediaTrack::SetEnabled);
}

VideoTrackFactory::VideoTrackFactory() {
  AddGenericProperty<VideoTrack>("kind", &MediaTrack::kind);
  AddGenericProperty<VideoTrack>("label", &MediaTrack::label);
  AddGenericProperty<VideoTrack>("language", &MediaTrack::language);
  AddGenericProperty<VideoTrack>("id", &MediaTrack::id);
  // The property is called "selected" on video tracks, but use the same fields
  // as audio for consistency.
  AddGenericProperty<VideoTrack>("selected", &MediaTrack::enabled,
                                 &MediaTrack::SetEnabled);
}

}  // namespace mse
}  // namespace js
}  // namespace shaka
