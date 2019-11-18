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

#include "src/js/js_error.h"
#include "src/js/mse/video_element.h"

namespace shaka {
namespace js {
namespace mse {

TextTrack::TextTrack(RefPtr<const HTMLVideoElement> video,
                     std::shared_ptr<shaka::media::TextTrack> track)
    : kind(track->kind),
      label(track->label),
      language(track->language),
      id(track->id),
      mutex_("TextTrack"),
      video_(video),
      track_(track) {
  track->AddClient(this);
}

TextTrack::~TextTrack() {
  track_->RemoveClient(this);
}

void TextTrack::Trace(memory::HeapTracer* tracer) const {
  events::EventTarget::Trace(tracer);

  std::unique_lock<Mutex> lock(mutex_);
  for (auto& pair : cues_)
    tracer->Trace(&pair.second);
  tracer->Trace(&video_);
}

std::vector<RefPtr<VTTCue>> TextTrack::cues() const {
  std::unique_lock<Mutex> lock(mutex_);
  std::vector<RefPtr<VTTCue>> ret;
  ret.reserve(cues_.size());
  for (auto& pair : cues_)
    ret.emplace_back(pair.second);

  return ret;
}

media::TextTrackMode TextTrack::mode() const {
  return track_->mode();
}

void TextTrack::SetMode(media::TextTrackMode mode) {
  track_->SetMode(mode);
}


void TextTrack::AddCue(RefPtr<VTTCue> cue) {
  // Don't add to |cues_| since we'll get an event for it anyway.
  track_->AddCue(cue->GetPublic());
}

void TextTrack::RemoveCue(RefPtr<VTTCue> cue) {
  // Don't change to |cues_| since we'll get an event for it anyway.
  track_->RemoveCue(cue->GetPublic());
}

void TextTrack::OnCueAdded(std::shared_ptr<shaka::media::VTTCue> cue) {
  std::unique_lock<Mutex> lock(mutex_);
  cues_.emplace(cue.get(), new VTTCue(cue));
}

void TextTrack::OnCueRemoved(std::shared_ptr<shaka::media::VTTCue> cue) {
  std::unique_lock<Mutex> lock(mutex_);
  cues_.erase(cue.get());
}


TextTrackFactory::TextTrackFactory() {
  AddReadOnlyProperty("kind", &TextTrack::kind);
  AddReadOnlyProperty("label", &TextTrack::label);
  AddReadOnlyProperty("language", &TextTrack::language);
  AddReadOnlyProperty("id", &TextTrack::id);

  AddGenericProperty("cues", &TextTrack::cues);
  AddGenericProperty("mode", &TextTrack::mode, &TextTrack::SetMode);

  AddMemberFunction("addCue", &TextTrack::AddCue);
  AddMemberFunction("removeCue", &TextTrack::RemoveCue);

  NotImplemented("activeCues");
  NotImplemented("oncuechange");
}

}  // namespace mse
}  // namespace js
}  // namespace shaka
