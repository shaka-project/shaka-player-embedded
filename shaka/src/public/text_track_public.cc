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

#include "shaka/text_track.h"
#include "src/js/mse/text_track.h"
#include "src/js/vtt_cue.h"
#include "src/util/js_wrapper.h"

namespace shaka {

using JSTextTrack = js::mse::TextTrack;

class TextTrack::Impl : public util::JSWrapper<JSTextTrack> {
 public:
  explicit Impl(JSTextTrack* inner) {
    this->inner = inner;
  }
};

TextTrack::TextTrack(js::mse::TextTrack* inner) : impl_(new Impl(inner)) {
  CHECK(inner) << "Must pass a TextTrack instance";
}

TextTrack& TextTrack::operator=(TextTrack&& other) {
  std::swap(impl_, other.impl_);
  return *this;
}

TextTrack::TextTrack(TextTrack&&) = default;

TextTrack::~TextTrack() {}

void TextTrack::SetCueChangeEventListener(std::function<void()> callback) {
  auto task = PlainCallbackTask([=]() {
    impl_->inner->SetCppEventListener(js::EventType::CueChange, callback);
  });
  const std::string task_name = "TextTrack SetCueChangeEventListener";
  JsManagerImpl::Instance()
      ->MainThread()
      ->AddInternalTask(TaskPriority::Internal, task_name, task)
      ->GetValue();
}

void TextTrack::UnsetCueChangeEventListener() {
  auto task = PlainCallbackTask(
      [=]() { impl_->inner->UnsetCppEventListener(js::EventType::CueChange); });
  const std::string task_name = "TextTrack UnsetCueChangeEventListener";
  JsManagerImpl::Instance()
      ->MainThread()
      ->AddInternalTask(TaskPriority::Internal, task_name, task)
      ->GetValue();
}

TextTrackKind TextTrack::kind() {
  return impl_->GetMemberVariable(&JSTextTrack::kind);
}

void TextTrack::SetKind(TextTrackKind kind) {
  impl_->SetMemberVariable(&JSTextTrack::kind, kind);
}

std::string TextTrack::label() {
  return impl_->GetMemberVariable(&JSTextTrack::label);
}

void TextTrack::SetLabel(const std::string label) {
  impl_->SetMemberVariable(&JSTextTrack::label, label);
}

std::string TextTrack::language() {
  return impl_->GetMemberVariable(&JSTextTrack::language);
}

void TextTrack::SetLanguage(const std::string language) {
  impl_->SetMemberVariable(&JSTextTrack::language, language);
}

std::string TextTrack::id() {
  return impl_->GetMemberVariable(&JSTextTrack::id);
}

void TextTrack::SetId(const std::string id) {
  impl_->SetMemberVariable(&JSTextTrack::id, id);
}

TextTrackMode TextTrack::mode() {
  return impl_->CallInnerMethod(&JSTextTrack::mode);
}

void TextTrack::SetMode(TextTrackMode mode) {
  impl_->CallInnerMethod(&JSTextTrack::SetMode, mode);
}

std::vector<VTTCue*> TextTrack::cues() {
  auto cues = impl_->GetMemberVariable(&JSTextTrack::cues);
  return std::vector<VTTCue*>(cues.begin(), cues.end());
}

void TextTrack::AddCue(const VTTCue& cue) {
  // Copy the cue into an inner cue.
  RefPtr<js::VTTCue> inner_cue = new js::VTTCue(cue);

  impl_->CallInnerMethod(&JSTextTrack::AddCue, inner_cue);
}

void TextTrack::RemoveCue(VTTCue* cue) {
  // Locate cue in list.
  auto cues = impl_->GetMemberVariable(&JSTextTrack::cues);
  auto inner_cue_iter = std::find(cues.begin(), cues.end(), cue);
  CHECK(inner_cue_iter != cues.end())
      << "Can only remove cues retrieved from cues list";
  if (inner_cue_iter != cues.end()) {
    RefPtr<js::VTTCue> inner_cue = *inner_cue_iter;
    impl_->CallInnerMethod(&JSTextTrack::RemoveCue, inner_cue);
  }
}

}  // namespace shaka
