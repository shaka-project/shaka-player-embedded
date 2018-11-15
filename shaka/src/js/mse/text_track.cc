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
      mode_(TextTrackMode::Hidden),
      mutex_("TextTrack") {
  AddListenerField(EventType::CueChange, &on_cue_change);
}

TextTrack::~TextTrack() {}


void TextTrack::AddCue(RefPtr<VTTCue> cue) {
  std::unique_lock<Mutex> lock(mutex_);
  cues.emplace_back(cue);
  if (mode_ != TextTrackMode::Disabled) {
    // TODO: Only fire event if new cue will be displayed.
    ScheduleEvent<events::Event>(EventType::CueChange);
  }
}

void TextTrack::RemoveCue(RefPtr<VTTCue> cue) {
  std::unique_lock<Mutex> lock(mutex_);
  cues.erase(std::remove(cues.begin(), cues.end(), cue), cues.end());
  if (mode_ != TextTrackMode::Disabled) {
    // TODO: Only fire event if old cue was being displayed.
    ScheduleEvent<events::Event>(EventType::CueChange);
  }
}

void TextTrack::CheckForCueChange(double newTime, double oldTime) {
  std::unique_lock<Mutex> lock(mutex_);
  if (mode_ == TextTrackMode::Disabled)
    return;

  const double earlier = std::min(oldTime, newTime);
  const double later = std::max(oldTime, newTime);
  for (const auto& cue : cues) {
    if ((cue->startTime() > earlier && cue->startTime() <= later) ||
        (cue->endTime() > earlier && cue->endTime() <= later)) {
      // A cue just began, or ended.
      ScheduleEvent<events::Event>(EventType::CueChange);
      return;
    }
  }
}

TextTrackMode TextTrack::mode() const {
  std::unique_lock<Mutex> lock(mutex_);
  return mode_;
}

void TextTrack::SetMode(TextTrackMode mode) {
  std::unique_lock<Mutex> lock(mutex_);
  mode_ = mode;
  ScheduleEvent<events::Event>(EventType::CueChange);
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

  AddListenerField(EventType::CueChange, &TextTrack::on_cue_change);

  NotImplemented("activeCues");
}

}  // namespace mse
}  // namespace js
}  // namespace shaka
