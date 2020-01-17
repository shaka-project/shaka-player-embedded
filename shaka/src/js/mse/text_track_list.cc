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

#include "src/js/mse/text_track_list.h"

#include "src/js/mse/text_track.h"

namespace shaka {
namespace js {
namespace mse {

TextTrackList::TextTrackList(const std::vector<RefPtr<TextTrack>>& tracks)
    : text_tracks_(tracks.begin(), tracks.end()) {}

// \cond Doxygen_Skip
TextTrackList::~TextTrackList() {}
// \endcond Doxygen_Skip

void TextTrackList::Trace(memory::HeapTracer* tracer) const {
  events::EventTarget::Trace(tracer);
  tracer->Trace(&text_tracks_);
}

bool TextTrackList::GetIndex(size_t i, RefPtr<TextTrack>* track) const {
  if (i < text_tracks_.size()) {
    *track = text_tracks_.at(i);
    return true;
  } else {
    return false;
  }
}


TextTrackListFactory::TextTrackListFactory() {
  AddGenericProperty("length", &TextTrackList::length);

  AddIndexer(&TextTrackList::GetIndex);
}

}  // namespace mse
}  // namespace js
}  // namespace shaka
