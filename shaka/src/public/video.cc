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

#include "shaka/video.h"

#include "src/core/js_manager_impl.h"
#include "src/js/dom/document.h"
#include "src/js/mse/media_source.h"
#include "src/js/mse/video_element.h"
#include "src/util/js_wrapper.h"

namespace shaka {

using JSVideo = js::mse::HTMLVideoElement;

class Video::Impl : public util::JSWrapper<JSVideo> {};

Video::Video(JsManager* engine) : impl_(new Impl) {
  CHECK(engine) << "Must pass a JsManager instance";
}

Video::Video(Video&&) = default;

Video::~Video() {}

Video& Video::operator=(Video&&) = default;

void Video::Initialize() {
  // This can be called immediately after the JsManager constructor.  Since the
  // Environment might not be setup yet, run this in an internal task so we know
  // it is ready.
  const auto callback = [this]() {
    impl_->inner =
        new js::mse::HTMLVideoElement(js::dom::Document::GetGlobalDocument());
  };
  JsManagerImpl::Instance()
      ->MainThread()
      ->AddInternalTask(TaskPriority::Internal, "Video init",
                        PlainCallbackTask(callback))
      ->GetValue();
}

Frame Video::DrawFrame(double* delay) {
  DCHECK(impl_->inner) << "Must call Initialize.";
  RefPtr<js::mse::MediaSource> source = impl_->inner->GetMediaSource();
  if (!source)
    return Frame();
  return source->GetController()->DrawFrame(delay);
}


double Video::Duration() const {
  return impl_->CallInnerMethod(&JSVideo::Duration);
}

bool Video::Ended() const {
  return impl_->CallInnerMethod(&JSVideo::Ended);
}

bool Video::Seeking() const {
  return impl_->CallInnerMethod(&JSVideo::Seeking);
}

bool Video::Paused() const {
  return impl_->CallInnerMethod(&JSVideo::Paused);
}

bool Video::Muted() const {
  return impl_->CallInnerMethod(&JSVideo::Muted);
}

void Video::SetMuted(bool muted) {
  impl_->CallInnerMethod(&JSVideo::SetMuted, muted);
}

std::vector<TextTrack> Video::TextTracks() {
  std::vector<Member<js::mse::TextTrack>> original =
      impl_->GetMemberVariable(&JSVideo::text_tracks);

  // For each element in [begin, end), transform it using |callback| and
  // insert the result at the back of |text_tracks|.
  std::vector<TextTrack> text_tracks;
  auto begin = original.begin();
  auto end = original.end();
  const auto callback = [](const Member<js::mse::TextTrack> track) {
    return TextTrack(track);
  };
  std::transform(begin, end, std::back_inserter(text_tracks), callback);
  return text_tracks;
}

double Video::Volume() const {
  return impl_->CallInnerMethod(&JSVideo::Volume);
}

void Video::SetVolume(double volume) {
  impl_->CallInnerMethod(&JSVideo::SetVolume, volume);
}

double Video::CurrentTime() const {
  return impl_->CallInnerMethod(&JSVideo::CurrentTime);
}

void Video::SetCurrentTime(double time) {
  impl_->CallInnerMethod(&JSVideo::SetCurrentTime, time);
}

double Video::PlaybackRate() const {
  return impl_->CallInnerMethod(&JSVideo::PlaybackRate);
}
void Video::SetPlaybackRate(double rate) {
  impl_->CallInnerMethod(&JSVideo::SetPlaybackRate, rate);
}

void Video::Play() {
  impl_->CallInnerMethod(&JSVideo::Play);
}

void Video::Pause() {
  impl_->CallInnerMethod(&JSVideo::Pause);
}

js::mse::HTMLVideoElement* Video::GetJavaScriptObject() {
  DCHECK(impl_->inner) << "Must call Initialize.";
  return impl_->inner;
}

}  // namespace shaka
