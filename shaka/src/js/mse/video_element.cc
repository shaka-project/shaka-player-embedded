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

#include "src/js/mse/video_element.h"

#include <cmath>

#include "src/core/js_manager_impl.h"
#include "src/js/dom/document.h"
#include "src/js/js_error.h"
#include "src/js/mse/media_source.h"
#include "src/js/mse/time_ranges.h"
#include "src/util/macros.h"
#include "src/util/utils.h"

namespace shaka {
namespace js {
namespace mse {

#define NOT_ATTACHED_ERROR                 \
  JsError::DOMException(InvalidStateError, \
                        "The video has been detached from the MediaPlayer")
#define CHECK_ATTACHED() \
  if (player_)           \
    ;                    \
  else                   \
    return NOT_ATTACHED_ERROR

BEGIN_ALLOW_COMPLEX_STATICS
std::unordered_set<HTMLVideoElement*> HTMLVideoElement::g_video_elements_;
END_ALLOW_COMPLEX_STATICS

HTMLVideoElement::HTMLVideoElement(RefPtr<dom::Document> document,
                                   media::MediaPlayer* player)
    : dom::Element(document, "video", nullopt, nullopt),
      autoplay(false),
      loop(false),
      default_muted(false),
      player_(player),
      clock_(&util::Clock::Instance) {
  AddListenerField(EventType::Encrypted, &on_encrypted);
  AddListenerField(EventType::WaitingForKey, &on_waiting_for_key);
  player_->AddClient(this);
  g_video_elements_.insert(this);
}

// \cond Doxygen_Skip
HTMLVideoElement::~HTMLVideoElement() {
  if (player_)
    Detach();
  g_video_elements_.erase(this);
}
// \endcond Doxygen_Skip

RefPtr<HTMLVideoElement> HTMLVideoElement::AnyVideoElement() {
  return *g_video_elements_.begin();
}

void HTMLVideoElement::Trace(memory::HeapTracer* tracer) const {
  dom::Element::Trace(tracer);
  tracer->Trace(&media_source_);
  for (auto& pair : text_track_cache_) {
    tracer->Trace(&pair.second);
  }
}

void HTMLVideoElement::Detach() {
  player_->RemoveClient(this);
  player_ = nullptr;
}


Promise HTMLVideoElement::SetMediaKeys(RefPtr<eme::MediaKeys> media_keys) {
  if (!player_)
    return Promise::Rejected(NOT_ATTACHED_ERROR);
  if (!media_keys && !media_source_)
    return Promise::Resolved();

  eme::Implementation* cdm = media_keys ? media_keys->GetCdm() : nullptr;
  std::string key_system = media_keys ? media_keys->key_system : "";
  player_->SetEmeImplementation(key_system, cdm);
  this->media_keys = media_keys;
  return Promise::Resolved();
}

ExceptionOr<void> HTMLVideoElement::Load() {
  CHECK_ATTACHED();
  error = nullptr;

  if (media_source_) {
    player_->Detach();
    media_source_->CloseMediaSource();
    media_source_.reset();
  } else if (!src_.empty()) {
    player_->Detach();
    src_ = "";
  }

  SetMuted(default_muted);
  return {};
}

CanPlayTypeEnum HTMLVideoElement::CanPlayType(const std::string& type) {
  if (!MediaSource::IsTypeSupported(type))
    return CanPlayTypeEnum::EMPTY;
  return CanPlayTypeEnum::MAYBE;
}

std::vector<RefPtr<TextTrack>> HTMLVideoElement::text_tracks() const {
  if (!player_)
    return {};

  auto player_tracks = player_->TextTracks();
  std::vector<RefPtr<TextTrack>> ret;
  ret.reserve(player_tracks.size());
  for (auto& track : player_tracks) {
    auto it = text_track_cache_.find(track);
    if (it != text_track_cache_.end()) {
      ret.emplace_back(it->second);
    } else {
      auto pair = text_track_cache_.emplace(track, new TextTrack(this, track));
      ret.emplace_back(pair.first->second);
    }
  }
  return ret;
}

media::VideoReadyState HTMLVideoElement::GetReadyState() const {
  if (!player_)
    return media::VideoReadyState::HaveNothing;

  auto ret = player_->ReadyState();
  if (ret == media::VideoReadyState::NotAttached)
    ret = media::VideoReadyState::HaveNothing;
  return ret;
}

ExceptionOr<media::VideoPlaybackQuality>
HTMLVideoElement::GetVideoPlaybackQuality() const {
  CHECK_ATTACHED();

  auto temp = player_->VideoPlaybackQuality();
  media::VideoPlaybackQuality ret;
  ret.totalVideoFrames = temp.total_video_frames;
  ret.droppedVideoFrames = temp.dropped_video_frames;
  ret.corruptedVideoFrames = temp.corrupted_video_frames;
  return ret;
}

RefPtr<TimeRanges> HTMLVideoElement::Buffered() const {
  return new TimeRanges(player_ ? player_->GetBuffered()
                                : std::vector<media::BufferedRange>{});
}

RefPtr<TimeRanges> HTMLVideoElement::Seekable() const {
  const double duration = Duration();
  media::BufferedRanges ranges;
  if (std::isfinite(duration))
    ranges.emplace_back(0, duration);
  return new TimeRanges(ranges);
}

std::string HTMLVideoElement::Source() const {
  return media_source_ ? media_source_->url : src_;
}

ExceptionOr<void> HTMLVideoElement::SetSource(const std::string& src) {
  // Unload any previous MediaSource objects.
  RETURN_IF_ERROR(Load());

  DCHECK(!media_source_);
  if (src.empty())
    return {};

  media_source_ = MediaSource::FindMediaSource(src);
  if (media_source_) {
    if (!player_->AttachMse()) {
      return JsError::DOMException(NotSupportedError,
                                   "Error attaching to MediaPlayer");
    }
    media_source_->OpenMediaSource(this, player_);

    if (autoplay)
      player_->Play();
  } else {
    if (!player_->AttachSource(src)) {
      return JsError::DOMException(NotSupportedError,
                                   "Given src= URL is unsupported");
    }
    src_ = src;
  }
  return {};
}

double HTMLVideoElement::CurrentTime() const {
  return player_ ? player_->CurrentTime() : 0;
}

ExceptionOr<void> HTMLVideoElement::SetCurrentTime(double time) {
  CHECK_ATTACHED();
  player_->SetCurrentTime(time);
  return {};
}

double HTMLVideoElement::Duration() const {
  return player_ ? player_->Duration() : 0;
}

double HTMLVideoElement::PlaybackRate() const {
  return player_ ? player_->PlaybackRate() : 0;
}

ExceptionOr<void> HTMLVideoElement::SetPlaybackRate(double rate) {
  CHECK_ATTACHED();
  player_->SetPlaybackRate(rate);
  return {};
}

bool HTMLVideoElement::Muted() const {
  return player_ && player_->Muted();
}

ExceptionOr<void> HTMLVideoElement::SetMuted(bool muted) {
  CHECK_ATTACHED();
  player_->SetMuted(muted);
  return {};
}

double HTMLVideoElement::Volume() const {
  return player_ ? player_->Volume() : 0;
}

ExceptionOr<void> HTMLVideoElement::SetVolume(double volume) {
  CHECK_ATTACHED();
  if (volume < 0 || volume > 1) {
    return JsError::DOMException(
        IndexSizeError,
        util::StringPrintf(
            "The volume provided (%f) is outside the range [0, 1].", volume));
  }

  player_->SetVolume(volume);
  return {};
}

bool HTMLVideoElement::Paused() const {
  if (!player_)
    return false;

  auto state = player_->PlaybackState();
  return state == media::VideoPlaybackState::Initializing ||
         state == media::VideoPlaybackState::Paused ||
         state == media::VideoPlaybackState::Ended;
}

bool HTMLVideoElement::Seeking() const {
  return player_ &&
         player_->PlaybackState() == media::VideoPlaybackState::Seeking;
}

bool HTMLVideoElement::Ended() const {
  return player_ &&
         player_->PlaybackState() == media::VideoPlaybackState::Ended;
}

ExceptionOr<void> HTMLVideoElement::Play() {
  CHECK_ATTACHED();
  player_->Play();
  return {};
}

ExceptionOr<void> HTMLVideoElement::Pause() {
  CHECK_ATTACHED();
  player_->Pause();
  return {};
}

ExceptionOr<RefPtr<TextTrack>> HTMLVideoElement::AddTextTrack(
    media::TextTrackKind kind, optional<std::string> label,
    optional<std::string> language) {
  CHECK_ATTACHED();

  auto track =
      player_->AddTextTrack(kind, label.value_or(""), language.value_or(""));
  if (!track) {
    return JsError::DOMException(UnknownError, "Error creating TextTrack");
  }

  RefPtr<TextTrack> ret(new TextTrack(this, track));
  text_track_cache_[track] = ret;
  return ret;
}

void HTMLVideoElement::OnReadyStateChanged(media::VideoReadyState old_state,
                                           media::VideoReadyState new_state) {
  if (old_state < media::VideoReadyState::HaveMetadata &&
      new_state >= media::VideoReadyState::HaveMetadata) {
    ScheduleEvent<events::Event>(EventType::LoadedMetaData);
  }
  if (old_state < media::VideoReadyState::HaveCurrentData &&
      new_state >= media::VideoReadyState::HaveCurrentData) {
    ScheduleEvent<events::Event>(EventType::LoadedData);
  }
  if (old_state < media::VideoReadyState::HaveFutureData &&
      new_state >= media::VideoReadyState::HaveFutureData) {
    ScheduleEvent<events::Event>(EventType::CanPlay);
  }
  if (old_state < media::VideoReadyState::HaveEnoughData &&
      new_state >= media::VideoReadyState::HaveEnoughData) {
    ScheduleEvent<events::Event>(EventType::CanPlayThrough);
  }

  if (old_state >= media::VideoReadyState::HaveFutureData &&
      new_state < media::VideoReadyState::HaveFutureData &&
      new_state > media::VideoReadyState::HaveNothing) {
    ScheduleEvent<events::Event>(EventType::Waiting);
  }

  ScheduleEvent<events::Event>(EventType::ReadyStateChange);
}

void HTMLVideoElement::OnPlaybackStateChanged(
    media::VideoPlaybackState old_state, media::VideoPlaybackState new_state) {
  switch (new_state) {
    case media::VideoPlaybackState::Detached:
      ScheduleEvent<events::Event>(EventType::Emptied);
      break;
    case media::VideoPlaybackState::Paused:
      ScheduleEvent<events::Event>(EventType::Pause);
      break;
    case media::VideoPlaybackState::Buffering:
      ScheduleEvent<events::Event>(EventType::Waiting);
      break;
    case media::VideoPlaybackState::Playing:
      ScheduleEvent<events::Event>(EventType::Playing);
      break;
    case media::VideoPlaybackState::Ended:
      ScheduleEvent<events::Event>(EventType::Ended);
      break;

    case media::VideoPlaybackState::Initializing:
      break;
    case media::VideoPlaybackState::Seeking:
      // We also get an OnSeeking callback, so raise event there.
      break;
    case media::VideoPlaybackState::WaitingForKey:
      // This happens multiple times, raise the event in the OnWaitingForKey.
      break;
  }
  if (old_state == media::VideoPlaybackState::Seeking)
    ScheduleEvent<events::Event>(EventType::Seeked);
}

void HTMLVideoElement::OnError(const std::string& error) {
  if (!this->error) {
    if (error.empty())
      this->error = new MediaError(MEDIA_ERR_DECODE, "Unknown media error");
    else
      this->error = new MediaError(MEDIA_ERR_DECODE, error);
  }
  ScheduleEvent<events::Event>(EventType::Error);
}

void HTMLVideoElement::OnPlay() {
  ScheduleEvent<events::Event>(EventType::Play);
}

void HTMLVideoElement::OnSeeking() {
  ScheduleEvent<events::Event>(EventType::Seeking);
}

void HTMLVideoElement::OnWaitingForKey() {
  ScheduleEvent<events::Event>(EventType::WaitingForKey);
}


HTMLVideoElementFactory::HTMLVideoElementFactory() {
  AddConstant("HAVE_NOTHING", media::VideoReadyState::HaveNothing);
  AddConstant("HAVE_METADATA", media::VideoReadyState::HaveMetadata);
  AddConstant("HAVE_CURRENT_DATA", media::VideoReadyState::HaveCurrentData);
  AddConstant("HAVE_FUTURE_DATA", media::VideoReadyState::HaveFutureData);
  AddConstant("HAVE_ENOUGH_DATA", media::VideoReadyState::HaveEnoughData);

  AddListenerField(EventType::Encrypted, &HTMLVideoElement::on_encrypted);
  AddListenerField(EventType::WaitingForKey,
                   &HTMLVideoElement::on_waiting_for_key);

  AddReadWriteProperty("autoplay", &HTMLVideoElement::autoplay);
  AddReadWriteProperty("loop", &HTMLVideoElement::loop);
  AddReadWriteProperty("defaultMuted", &HTMLVideoElement::default_muted);
  AddReadOnlyProperty("mediaKeys", &HTMLVideoElement::media_keys);
  AddReadOnlyProperty("error", &HTMLVideoElement::error);

  AddGenericProperty("textTracks", &HTMLVideoElement::text_tracks);
  AddGenericProperty("readyState", &HTMLVideoElement::GetReadyState);
  AddGenericProperty("paused", &HTMLVideoElement::Paused);
  AddGenericProperty("seeking", &HTMLVideoElement::Seeking);
  AddGenericProperty("ended", &HTMLVideoElement::Ended);
  AddGenericProperty("buffered", &HTMLVideoElement::Buffered);
  AddGenericProperty("seekable", &HTMLVideoElement::Seekable);
  AddGenericProperty("src", &HTMLVideoElement::Source,
                     &HTMLVideoElement::SetSource);
  AddGenericProperty("currentSrc", &HTMLVideoElement::Source);
  AddGenericProperty("currentTime", &HTMLVideoElement::CurrentTime,
                     &HTMLVideoElement::SetCurrentTime);
  AddGenericProperty("duration", &HTMLVideoElement::Duration);
  AddGenericProperty("playbackRate", &HTMLVideoElement::PlaybackRate,
                     &HTMLVideoElement::SetPlaybackRate);
  AddGenericProperty("volume", &HTMLVideoElement::Volume,
                     &HTMLVideoElement::SetVolume);
  AddGenericProperty("muted", &HTMLVideoElement::Muted,
                     &HTMLVideoElement::SetMuted);

  AddMemberFunction("load", &HTMLVideoElement::Load);
  AddMemberFunction("play", &HTMLVideoElement::Play);
  AddMemberFunction("pause", &HTMLVideoElement::Pause);
  AddMemberFunction("setMediaKeys", &HTMLVideoElement::SetMediaKeys);
  AddMemberFunction("addTextTrack", &HTMLVideoElement::AddTextTrack);
  AddMemberFunction("getVideoPlaybackQuality",
                    &HTMLVideoElement::GetVideoPlaybackQuality);
  AddMemberFunction("canPlayType", &HTMLVideoElement::CanPlayType);

  NotImplemented("crossOrigin");
  NotImplemented("networkState");
  NotImplemented("preload");
  NotImplemented("getStartDate");
  NotImplemented("defaultPlaybackRate");
  NotImplemented("playable");
  NotImplemented("mediaGroup");
  NotImplemented("controller");
  NotImplemented("controls");
  NotImplemented("audioTracks");
  NotImplemented("videoTracks");
}

}  // namespace mse
}  // namespace js
}  // namespace shaka
