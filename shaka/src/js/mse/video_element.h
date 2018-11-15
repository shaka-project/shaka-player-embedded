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

#ifndef SHAKA_EMBEDDED_JS_MSE_VIDEO_ELEMENT_H_
#define SHAKA_EMBEDDED_JS_MSE_VIDEO_ELEMENT_H_

#include <string>
#include <vector>

#include "shaka/optional.h"
#include "shaka/video.h"
#include "src/core/ref_ptr.h"
#include "src/debug/thread.h"
#include "src/js/dom/element.h"
#include "src/js/eme/media_keys.h"
#include "src/js/mse/media_error.h"
#include "src/js/mse/text_track.h"
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

class HTMLVideoElement : public dom::Element {
  DECLARE_TYPE_INFO(HTMLVideoElement);

 public:
  explicit HTMLVideoElement(RefPtr<dom::Document> document);

  void Trace(memory::HeapTracer* tracer) const override;

  void OnReadyStateChanged(media::MediaReadyState new_ready_state);
  void OnPipelineStatusChanged(media::PipelineStatus status);
  void OnMediaError(media::SourceType source, media::Status status);
  void CheckForCueChange(double newTime, double oldTime);

  RefPtr<MediaSource> GetMediaSource() const;

  // Encrypted media extensions
  Promise SetMediaKeys(RefPtr<eme::MediaKeys> media_keys);
  Member<eme::MediaKeys> media_keys;
  Listener on_encrypted;
  Listener on_waiting_for_key;

  // HTMLMediaElement members.
  void Load();
  static CanPlayTypeEnum CanPlayType(const std::string& type);

  media::MediaReadyState ready_state;
  bool autoplay;
  bool loop;
  std::vector<Member<TextTrack>> text_tracks;
  RefPtr<MediaError> error;

  media::VideoPlaybackQuality GetVideoPlaybackQuality() const;
  RefPtr<TimeRanges> Buffered() const;
  std::string Source() const;
  ExceptionOr<void> SetSource(const std::string& src);
  double CurrentTime() const;
  void SetCurrentTime(double time);
  double Duration() const;
  double PlaybackRate() const;
  void SetPlaybackRate(double rate);
  bool Muted() const;
  void SetMuted(bool muted);
  double Volume() const;
  void SetVolume(double volume);

  bool Paused() const;
  bool Seeking() const;
  bool Ended() const;

  void Play();
  void Pause();
  RefPtr<TextTrack> AddTextTrack(TextTrackKind kind,
                                 optional<std::string> label,
                                 optional<std::string> language);

 private:
  void ThreadMain();

  Member<MediaSource> media_source_;
  media::PipelineStatus pipeline_status_;
  double volume_;
  bool will_play_;
  bool is_muted_;
  const util::Clock* const clock_;
  std::atomic<bool> shutdown_;
  Thread thread_;
};

class HTMLVideoElementFactory
    : public BackingObjectFactory<HTMLVideoElement, dom::Element> {
 public:
  HTMLVideoElementFactory();
};

}  // namespace mse
}  // namespace js
}  // namespace shaka

CONVERT_ENUM_AS_NUMBER(shaka::media, MediaReadyState);

DEFINE_ENUM_MAPPING(shaka::js::mse, CanPlayTypeEnum) {
  AddMapping(Enum::EMPTY, "");
  AddMapping(Enum::MAYBE, "maybe");
  AddMapping(Enum::PROBABLY, "probably");
}

#endif  // SHAKA_EMBEDDED_JS_MSE_VIDEO_ELEMENT_H_
