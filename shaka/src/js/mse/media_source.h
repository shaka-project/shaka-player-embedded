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

#include "shaka/optional.h"
#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/js/events/event_target.h"
#include "src/mapping/byte_buffer.h"
#include "src/mapping/enum.h"
#include "src/mapping/exception_or.h"
#include "src/media/types.h"
#include "src/media/video_controller.h"

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

class MediaSource : public events::EventTarget {
  DECLARE_TYPE_INFO(MediaSource);

 public:
  MediaSource();

  static MediaSource* Create() {
    return new MediaSource();
  }

  static bool IsTypeSupported(const std::string& mime_type);
  static RefPtr<MediaSource> FindMediaSource(const std::string& url);

  media::VideoController* GetController() {
    return &controller_;
  }

  void Trace(memory::HeapTracer* tracer) const override;

  ExceptionOr<RefPtr<SourceBuffer>> AddSourceBuffer(const std::string& type);
  ExceptionOr<void> EndOfStream(optional<std::string> error);

  double GetDuration() const;
  ExceptionOr<void> SetDuration(double duration);


  /** Called when this MediaSource gets attached to a video element. */
  void OpenMediaSource(RefPtr<HTMLVideoElement> video);
  /** Called when the media source gets detached. */
  void CloseMediaSource();


  Listener on_source_open;
  Listener on_source_ended;
  Listener on_source_close;

  MediaSourceReadyState ready_state;
  const std::string url;

 private:
  /** Called when the media's ready-state changes. */
  void OnReadyStateChanged(media::MediaReadyState ready_state);
  /** Called when the media pipeline status changes. */
  void OnPipelineStatusChanged(media::PipelineStatus status);
  /** Called when a media error occurs. */
  void OnMediaError(media::SourceType source, media::Status error);
  /** Called when the media pipeline is waiting for an EME key. */
  void OnWaitingForKey();
  /** Called when we get new encrypted initialization data. */
  void OnEncrypted(shaka::eme::MediaKeyInitDataType init_data_type,
                   ByteBuffer& init_data);

  std::unordered_map<media::SourceType, Member<SourceBuffer>> source_buffers_;
  media::VideoController controller_;
  Member<HTMLVideoElement> video_element_;

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
