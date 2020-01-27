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

#include "src/js/mse/track_list.h"

#include "shaka/media/media_track.h"
#include "shaka/media/text_track.h"
#include "src/js/mse/media_track.h"
#include "src/js/mse/text_track.h"

namespace shaka {
namespace js {
namespace mse {

// \cond Doxygen_Skip
AudioTrackList::~AudioTrackList() {}
// \endcond Doxygen_Skip

void AudioTrackList::OnAddAudioTrack(std::shared_ptr<media::MediaTrack> track) {
  AddTrack(track);
}

void AudioTrackList::OnRemoveAudioTrack(
    std::shared_ptr<media::MediaTrack> track) {
  RemoveTrack(track);
}


// \cond Doxygen_Skip
VideoTrackList::~VideoTrackList() {}
// \endcond Doxygen_Skip

void VideoTrackList::OnAddVideoTrack(std::shared_ptr<media::MediaTrack> track) {
  AddTrack(track);
}

void VideoTrackList::OnRemoveVideoTrack(
    std::shared_ptr<media::MediaTrack> track) {
  RemoveTrack(track);
}


// \cond Doxygen_Skip
TextTrackList::~TextTrackList() {}
// \endcond Doxygen_Skip

void TextTrackList::OnAddTextTrack(std::shared_ptr<media::TextTrack> track) {
  AddTrack(track);
}

void TextTrackList::OnRemoveTextTrack(std::shared_ptr<media::TextTrack> track) {
  RemoveTrack(track);
}

}  // namespace mse
}  // namespace js
}  // namespace shaka
