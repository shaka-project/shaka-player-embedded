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

#ifndef SHAKA_EMBEDDED_JS_MSE_MEDIA_SOURCE_H_
#define SHAKA_EMBEDDED_JS_MSE_MEDIA_SOURCE_H_

#include <string>
#include <unordered_map>

#include "shaka/media/demuxer.h"
#include "shaka/media/media_player.h"
#include "shaka/optional.h"
#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/js/events/event_target.h"
#include "src/mapping/byte_buffer.h"
#include "src/mapping/enum.h"
#include "src/mapping/exception_or.h"
#include "src/media/types.h"

namespace shaka {
namespace js {
namespace mse {

class HTMLVideoElement;
class SourceBuffer;


enum class MediaSourceReadyState {
  CLOSED,
  OPEN,
  ENDED,
};

class MediaSource : public events::EventTarget, public media::Demuxer::Client {
  DECLARE_TYPE_INFO(MediaSource);

 public:
  MediaSource();

  static MediaSource* Create() {
    return new MediaSource();
  }

  static bool IsTypeSupported(const std::string& mime_type);
  static RefPtr<MediaSource> FindMediaSource(const std::string& url);

  void Trace(memory::HeapTracer* tracer) const override;

  ExceptionOr<RefPtr<SourceBuffer>> AddSourceBuffer(const std::string& type);
  ExceptionOr<void> EndOfStream(optional<std::string> error);

  double GetDuration() const;
  ExceptionOr<void> SetDuration(double duration);


  /** Called when this MediaSource gets attached to a video element. */
  void OpenMediaSource(RefPtr<HTMLVideoElement> video,
                       media::MediaPlayer* player);
  /** Called when the media source gets detached. */
  void CloseMediaSource();


  Listener on_source_open;
  Listener on_source_ended;
  Listener on_source_close;

  MediaSourceReadyState ready_state;
  const std::string url;

 private:
  void OnLoadedMetaData(double duration) override;
  void OnEncrypted(::shaka::eme::MediaKeyInitDataType type, const uint8_t* data,
                   size_t size) override;

  Member<SourceBuffer> audio_buffer_;
  Member<SourceBuffer> video_buffer_;
  Member<HTMLVideoElement> video_;
  media::MediaPlayer* player_;
  bool got_loaded_metadata_;

  // A map of every MediaSource object created.  These are not traced so they
  // are weak pointers.  Once a MediaSource gets destroyed, it will be removed
  // from this map by its destructor.
  static std::unordered_map<std::string, Member<MediaSource>> media_sources_;
};

class MediaSourceFactory
    : public BackingObjectFactory<MediaSource, events::EventTarget> {
 public:
  MediaSourceFactory();
};

}  // namespace mse
}  // namespace js
}  // namespace shaka

DEFINE_ENUM_MAPPING(shaka::js::mse, MediaSourceReadyState) {
  AddMapping(Enum::CLOSED, "closed");
  AddMapping(Enum::OPEN, "open");
  AddMapping(Enum::ENDED, "ended");
}

#endif  // SHAKA_EMBEDDED_JS_MSE_MEDIA_SOURCE_H_
