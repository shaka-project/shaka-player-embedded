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

#ifndef SHAKA_EMBEDDED_JS_MSE_TEXT_TRACK_H_
#define SHAKA_EMBEDDED_JS_MSE_TEXT_TRACK_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "shaka/media/text_track.h"
#include "shaka/media/vtt_cue.h"
#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/js/events/event_target.h"
#include "src/js/vtt_cue.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"

namespace shaka {
namespace js {
namespace mse {

class HTMLVideoElement;

class TextTrack : public events::EventTarget, shaka::media::TextTrack::Client {
  DECLARE_TYPE_INFO(TextTrack);

 public:
  explicit TextTrack(RefPtr<const HTMLVideoElement> video,
                     std::shared_ptr<shaka::media::TextTrack> track);

  void Trace(memory::HeapTracer* tracer) const override;

  const media::TextTrackKind kind;
  const std::string label;
  const std::string language;
  const std::string id;

  std::vector<RefPtr<VTTCue>> cues() const;

  media::TextTrackMode mode() const;
  void SetMode(media::TextTrackMode mode);

  // Technically this should accept a TextTrackCue, but we don't distinguish
  // between the types.
  void AddCue(RefPtr<VTTCue> cue);
  void RemoveCue(RefPtr<VTTCue> cue);

 private:
  void OnCueAdded(std::shared_ptr<shaka::media::VTTCue> cue) override;
  void OnCueRemoved(std::shared_ptr<shaka::media::VTTCue> cue) override;

  mutable Mutex mutex_;
  std::unordered_map<shaka::media::VTTCue*, Member<VTTCue>> cues_;
  Member<const HTMLVideoElement> video_;
  std::shared_ptr<shaka::media::TextTrack> track_;
};

class TextTrackFactory
    : public BackingObjectFactory<TextTrack, events::EventTarget> {
 public:
  TextTrackFactory();
};

}  // namespace mse
}  // namespace js
}  // namespace shaka

DEFINE_ENUM_MAPPING(shaka::media, TextTrackKind) {
  AddMapping(Enum::Subtitles, "subtitles");
  AddMapping(Enum::Captions, "captions");
  AddMapping(Enum::Descriptions, "descriptions");
  AddMapping(Enum::Chapters, "chapters");
  AddMapping(Enum::Metadata, "metadata");
}

DEFINE_ENUM_MAPPING(shaka::media, TextTrackMode) {
  AddMapping(Enum::Disabled, "disabled");
  AddMapping(Enum::Hidden, "hidden");
  AddMapping(Enum::Showing, "showing");
}

#endif  // SHAKA_EMBEDDED_JS_MSE_TEXT_TRACK_H_
