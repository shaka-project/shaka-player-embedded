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

#ifndef SHAKA_EMBEDDED_JS_MSE_MEDIA_ELEMENT_H_
#define SHAKA_EMBEDDED_JS_MSE_MEDIA_ELEMENT_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "shaka/media/media_player.h"
#include "shaka/optional.h"
#include "src/core/ref_ptr.h"
#include "src/js/dom/element.h"
#include "src/js/eme/media_keys.h"
#include "src/js/mse/media_error.h"
#include "src/js/mse/media_track.h"
#include "src/js/mse/text_track.h"
#include "src/js/mse/track_list.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/enum.h"
#include "src/mapping/exception_or.h"
#include "src/mapping/promise.h"
#include "src/media/types.h"
#include "src/util/clock.h"

namespace shaka {
namespace js {
namespace mse {

class MediaSource;
class TimeRanges;

enum class CanPlayTypeEnum {
  EMPTY,
  MAYBE,
  PROBABLY,
};

class HTMLMediaElement : public dom::Element, media::MediaPlayer::Client {
  DECLARE_TYPE_INFO(HTMLMediaElement);

 public:
  HTMLMediaElement(RefPtr<dom::Document> document,
                   const std::string& name,
                   media::MediaPlayer* player);

  void Trace(memory::HeapTracer* tracer) const override;

  void Detach();

  // Encrypted media extensions
  Promise SetMediaKeys(RefPtr<eme::MediaKeys> media_keys);
  Member<eme::MediaKeys> media_keys;
  Listener on_encrypted;
  Listener on_waiting_for_key;

  // HTMLMediaElement members.
  ExceptionOr<void> Load();
  CanPlayTypeEnum CanPlayType(const std::string& type);

  bool autoplay;
  bool loop;
  bool default_muted;
  Member<MediaError> error;
  Member<AudioTrackList> audio_tracks;
  Member<VideoTrackList> video_tracks;
  Member<TextTrackList> text_tracks;

  media::VideoReadyState GetReadyState() const;
  RefPtr<TimeRanges> Buffered() const;
  RefPtr<TimeRanges> Seekable() const;
  std::string Source() const;
  ExceptionOr<void> SetSource(const std::string& src);
  double CurrentTime() const;
  ExceptionOr<void> SetCurrentTime(double time);
  double Duration() const;
  double PlaybackRate() const;
  ExceptionOr<void> SetPlaybackRate(double rate);
  double DefaultPlaybackRate() const;
  ExceptionOr<void> SetDefaultPlaybackRate(double rate);
  bool Muted() const;
  ExceptionOr<void> SetMuted(bool muted);
  double Volume() const;
  ExceptionOr<void> SetVolume(double volume);

  bool Paused() const;
  bool Seeking() const;
  bool Ended() const;

  ExceptionOr<void> Play();
  ExceptionOr<void> Pause();
  ExceptionOr<RefPtr<TextTrack>> AddTextTrack(media::TextTrackKind kind,
                                              optional<std::string> label,
                                              optional<std::string> language);

 protected:
  media::MediaPlayer* player_;

 private:
  void OnReadyStateChanged(media::VideoReadyState old_state,
                           media::VideoReadyState new_state) override;
  void OnPlaybackStateChanged(media::VideoPlaybackState old_state,
                              media::VideoPlaybackState new_state) override;
  void OnPlaybackRateChanged(double old_rate, double new_rate) override;
  void OnError(const std::string& error) override;
  void OnPlay() override;
  void OnSeeking() override;
  void OnWaitingForKey() override;

  Member<MediaSource> media_source_;
  const util::Clock* const clock_;
  std::string src_;
  double default_playback_rate_;
};

class HTMLMediaElementFactory
    : public BackingObjectFactory<HTMLMediaElement, dom::Element> {
 public:
  HTMLMediaElementFactory();
};

}  // namespace mse
}  // namespace js
}  // namespace shaka

CONVERT_ENUM_AS_NUMBER(shaka::media, VideoReadyState);

DEFINE_ENUM_MAPPING(shaka::js::mse, CanPlayTypeEnum) {
  AddMapping(Enum::EMPTY, "");
  AddMapping(Enum::MAYBE, "maybe");
  AddMapping(Enum::PROBABLY, "probably");
}

#endif  // SHAKA_EMBEDDED_JS_MSE_MEDIA_ELEMENT_H_
