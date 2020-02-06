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

#include "src/js/mse/media_element.h"

#include <cmath>

#include "src/core/js_manager_impl.h"
#include "src/js/dom/document.h"
#include "src/js/js_error.h"
#include "src/js/mse/media_source.h"
#include "src/js/mse/time_ranges.h"
#include "src/media/media_utils.h"
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

HTMLMediaElement::HTMLMediaElement(RefPtr<dom::Document> document,
                                   const std::string& name,
                                   media::MediaPlayer* player)
    : dom::Element(document, name, nullopt, nullopt),
      autoplay(false),
      loop(false),
      default_muted(false),
      audio_tracks(new AudioTrackList(player)),
      video_tracks(new VideoTrackList(player)),
      text_tracks(new TextTrackList(player)),
      player_(player),
      clock_(&util::Clock::Instance) {
  AddListenerField(EventType::Encrypted, &on_encrypted);
  AddListenerField(EventType::WaitingForKey, &on_waiting_for_key);
  player_->AddClient(this);
}

// \cond Doxygen_Skip
HTMLMediaElement::~HTMLMediaElement() {
  if (player_)
    Detach();
}
// \endcond Doxygen_Skip

void HTMLMediaElement::Trace(memory::HeapTracer* tracer) const {
  dom::Element::Trace(tracer);
  tracer->Trace(&error);
  tracer->Trace(&media_source_);
  tracer->Trace(&audio_tracks);
  tracer->Trace(&video_tracks);
  tracer->Trace(&text_tracks);
}

void HTMLMediaElement::Detach() {
  player_->RemoveClient(this);
  player_ = nullptr;

  audio_tracks->Detach();
  video_tracks->Detach();
  text_tracks->Detach();
}


Promise HTMLMediaElement::SetMediaKeys(RefPtr<eme::MediaKeys> media_keys) {
  if (!player_)
    return Promise::Rejected(NOT_ATTACHED_ERROR);
  if (!media_keys && !media_source_)
    return Promise::Resolved();

  eme::Implementation* cdm = media_keys ? media_keys->GetCdm() : nullptr;
  const std::string key_system = media_keys ? media_keys->key_system : "";
  if (!player_->SetEmeImplementation(key_system, cdm)) {
    return Promise::Rejected(
        JsError::TypeError("Error changing MediaKeys on the MediaPlayer"));
  }

  this->media_keys = media_keys;
  return Promise::Resolved();
}

ExceptionOr<void> HTMLMediaElement::Load() {
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

CanPlayTypeEnum HTMLMediaElement::CanPlayType(const std::string& type) {
  auto info =
      ConvertMimeToDecodingConfiguration(type, media::MediaDecodingType::File);
  if (!player_)
    return CanPlayTypeEnum::EMPTY;

  auto support = player_->DecodingInfo(info);
  if (!support.supported)
    return CanPlayTypeEnum::EMPTY;
  else if (!support.smooth)
    return CanPlayTypeEnum::MAYBE;
  return CanPlayTypeEnum::PROBABLY;
}

media::VideoReadyState HTMLMediaElement::GetReadyState() const {
  if (!player_)
    return media::VideoReadyState::HaveNothing;

  auto ret = player_->ReadyState();
  if (ret == media::VideoReadyState::NotAttached)
    ret = media::VideoReadyState::HaveNothing;
  return ret;
}

RefPtr<TimeRanges> HTMLMediaElement::Buffered() const {
  return new TimeRanges(player_ ? player_->GetBuffered()
                                : std::vector<media::BufferedRange>{});
}

RefPtr<TimeRanges> HTMLMediaElement::Seekable() const {
  const double duration = Duration();
  media::BufferedRanges ranges;
  if (std::isfinite(duration))
    ranges.emplace_back(0, duration);
  return new TimeRanges(ranges);
}

std::string HTMLMediaElement::Source() const {
  return media_source_ ? media_source_->url : src_;
}

ExceptionOr<void> HTMLMediaElement::SetSource(const std::string& src) {
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

double HTMLMediaElement::CurrentTime() const {
  return player_ ? player_->CurrentTime() : 0;
}

ExceptionOr<void> HTMLMediaElement::SetCurrentTime(double time) {
  CHECK_ATTACHED();
  player_->SetCurrentTime(time);
  return {};
}

double HTMLMediaElement::Duration() const {
  return player_ ? player_->Duration() : 0;
}

double HTMLMediaElement::PlaybackRate() const {
  return player_ ? player_->PlaybackRate() : 0;
}

ExceptionOr<void> HTMLMediaElement::SetPlaybackRate(double rate) {
  CHECK_ATTACHED();
  player_->SetPlaybackRate(rate);
  return {};
}

bool HTMLMediaElement::Muted() const {
  return player_ && player_->Muted();
}

ExceptionOr<void> HTMLMediaElement::SetMuted(bool muted) {
  CHECK_ATTACHED();
  player_->SetMuted(muted);
  return {};
}

double HTMLMediaElement::Volume() const {
  return player_ ? player_->Volume() : 0;
}

ExceptionOr<void> HTMLMediaElement::SetVolume(double volume) {
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

bool HTMLMediaElement::Paused() const {
  if (!player_)
    return false;

  auto state = player_->PlaybackState();
  return state == media::VideoPlaybackState::Initializing ||
         state == media::VideoPlaybackState::Paused ||
         state == media::VideoPlaybackState::Ended;
}

bool HTMLMediaElement::Seeking() const {
  return player_ &&
         player_->PlaybackState() == media::VideoPlaybackState::Seeking;
}

bool HTMLMediaElement::Ended() const {
  return player_ &&
         player_->PlaybackState() == media::VideoPlaybackState::Ended;
}

ExceptionOr<void> HTMLMediaElement::Play() {
  CHECK_ATTACHED();
  player_->Play();
  return {};
}

ExceptionOr<void> HTMLMediaElement::Pause() {
  CHECK_ATTACHED();
  player_->Pause();
  return {};
}

ExceptionOr<RefPtr<TextTrack>> HTMLMediaElement::AddTextTrack(
    media::TextTrackKind kind, optional<std::string> label,
    optional<std::string> language) {
  CHECK_ATTACHED();

  auto track =
      player_->AddTextTrack(kind, label.value_or(""), language.value_or(""));
  if (!track) {
    return JsError::DOMException(UnknownError, "Error creating TextTrack");
  }

  // The TextTrackList should already have gotten an event callback for the new
  // track, so the JS object should be in the list.
  RefPtr<TextTrack> ret = text_tracks->GetTrack(track);
  DCHECK(ret);
  return ret;
}

void HTMLMediaElement::OnReadyStateChanged(media::VideoReadyState old_state,
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

void HTMLMediaElement::OnPlaybackStateChanged(
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
    case media::VideoPlaybackState::Errored:
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

void HTMLMediaElement::OnError(const std::string& error) {
  if (!this->error) {
    if (error.empty())
      this->error = new MediaError(MEDIA_ERR_DECODE, "Unknown media error");
    else
      this->error = new MediaError(MEDIA_ERR_DECODE, error);
  }
  ScheduleEvent<events::Event>(EventType::Error);
}

void HTMLMediaElement::OnPlay() {
  ScheduleEvent<events::Event>(EventType::Play);
}

void HTMLMediaElement::OnSeeking() {
  ScheduleEvent<events::Event>(EventType::Seeking);
}

void HTMLMediaElement::OnWaitingForKey() {
  ScheduleEvent<events::Event>(EventType::WaitingForKey);
}


HTMLMediaElementFactory::HTMLMediaElementFactory() {
  AddConstant("HAVE_NOTHING", media::VideoReadyState::HaveNothing);
  AddConstant("HAVE_METADATA", media::VideoReadyState::HaveMetadata);
  AddConstant("HAVE_CURRENT_DATA", media::VideoReadyState::HaveCurrentData);
  AddConstant("HAVE_FUTURE_DATA", media::VideoReadyState::HaveFutureData);
  AddConstant("HAVE_ENOUGH_DATA", media::VideoReadyState::HaveEnoughData);

  AddListenerField(EventType::Encrypted, &HTMLMediaElement::on_encrypted);
  AddListenerField(EventType::WaitingForKey,
                   &HTMLMediaElement::on_waiting_for_key);

  AddReadWriteProperty("autoplay", &HTMLMediaElement::autoplay);
  AddReadWriteProperty("loop", &HTMLMediaElement::loop);
  AddReadWriteProperty("defaultMuted", &HTMLMediaElement::default_muted);
  AddReadOnlyProperty("mediaKeys", &HTMLMediaElement::media_keys);
  AddReadOnlyProperty("error", &HTMLMediaElement::error);
  AddReadOnlyProperty("audioTracks", &HTMLMediaElement::audio_tracks);
  AddReadOnlyProperty("videoTracks", &HTMLMediaElement::video_tracks);
  AddReadOnlyProperty("textTracks", &HTMLMediaElement::text_tracks);

  AddGenericProperty("readyState", &HTMLMediaElement::GetReadyState);
  AddGenericProperty("paused", &HTMLMediaElement::Paused);
  AddGenericProperty("seeking", &HTMLMediaElement::Seeking);
  AddGenericProperty("ended", &HTMLMediaElement::Ended);
  AddGenericProperty("buffered", &HTMLMediaElement::Buffered);
  AddGenericProperty("seekable", &HTMLMediaElement::Seekable);
  AddGenericProperty("src", &HTMLMediaElement::Source,
                     &HTMLMediaElement::SetSource);
  AddGenericProperty("currentSrc", &HTMLMediaElement::Source);
  AddGenericProperty("currentTime", &HTMLMediaElement::CurrentTime,
                     &HTMLMediaElement::SetCurrentTime);
  AddGenericProperty("duration", &HTMLMediaElement::Duration);
  AddGenericProperty("playbackRate", &HTMLMediaElement::PlaybackRate,
                     &HTMLMediaElement::SetPlaybackRate);
  AddGenericProperty("volume", &HTMLMediaElement::Volume,
                     &HTMLMediaElement::SetVolume);
  AddGenericProperty("muted", &HTMLMediaElement::Muted,
                     &HTMLMediaElement::SetMuted);

  AddMemberFunction("load", &HTMLMediaElement::Load);
  AddMemberFunction("play", &HTMLMediaElement::Play);
  AddMemberFunction("pause", &HTMLMediaElement::Pause);
  AddMemberFunction("setMediaKeys", &HTMLMediaElement::SetMediaKeys);
  AddMemberFunction("addTextTrack", &HTMLMediaElement::AddTextTrack);
  AddMemberFunction("canPlayType", &HTMLMediaElement::CanPlayType);

  NotImplemented("crossOrigin");
  NotImplemented("networkState");
  NotImplemented("preload");
  NotImplemented("getStartDate");
  NotImplemented("defaultPlaybackRate");
  NotImplemented("playable");
  NotImplemented("mediaGroup");
  NotImplemented("controller");
  NotImplemented("controls");
}

}  // namespace mse
}  // namespace js
}  // namespace shaka
