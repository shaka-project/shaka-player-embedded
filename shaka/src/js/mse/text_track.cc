// Copyright 2016 Google LLC
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

#include "src/js/mse/text_track.h"

#include <algorithm>

#include "src/mapping/enum.h"

namespace shaka {
namespace js {
namespace mse {

TextTrack::TextTrack(TextTrackKind kind, const std::string& label,
                     const std::string& language)
    : kind(kind),
      label(label),
      language(language),
      mode_(TextTrackMode::Hidden) {}

TextTrack::~TextTrack() {}


void TextTrack::AddCue(RefPtr<VTTCue> cue) {
  cues.emplace_back(cue);
}

void TextTrack::RemoveCue(RefPtr<VTTCue> cue) {
  cues.erase(std::remove(cues.begin(), cues.end(), cue), cues.end());
}

TextTrackMode TextTrack::mode() const {
  return mode_;
}

void TextTrack::SetMode(TextTrackMode mode) {
  mode_ = mode;
}

TextTrackFactory::TextTrackFactory() {
  AddReadOnlyProperty("kind", &TextTrack::kind);
  AddReadOnlyProperty("label", &TextTrack::label);
  AddReadOnlyProperty("language", &TextTrack::language);
  AddReadOnlyProperty("id", &TextTrack::id);
  AddReadOnlyProperty("cues", &TextTrack::cues);

  AddGenericProperty("mode", &TextTrack::mode, &TextTrack::SetMode);

  AddMemberFunction("addCue", &TextTrack::AddCue);
  AddMemberFunction("removeCue", &TextTrack::RemoveCue);

  NotImplemented("activeCues");
  NotImplemented("oncuechange");
}

}  // namespace mse
}  // namespace js
}  // namespace shaka
