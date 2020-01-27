// Copyright 2020 Google LLC
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

#ifndef SHAKA_EMBEDDED_JS_MSE_MEDIA_TRACK_H_
#define SHAKA_EMBEDDED_JS_MSE_MEDIA_TRACK_H_

#include <memory>
#include <string>

#include "shaka/media/media_track.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/enum.h"

namespace shaka {
namespace js {
namespace mse {

class MediaTrack : public BackingObject {
 public:
  explicit MediaTrack(std::shared_ptr<media::MediaTrack> track);
  ~MediaTrack() override;

  const std::string& label() const;
  const std::string& language() const;
  const std::string& id() const;
  media::MediaTrackKind kind() const;
  bool enabled() const;
  void SetEnabled(bool enabled);

 private:
  std::shared_ptr<media::MediaTrack> track_;
};

/**
 * @see https://html.spec.whatwg.org/multipage/media.html#audiotrack
 */
class AudioTrack : public MediaTrack {
  DECLARE_TYPE_INFO(AudioTrack);

 public:
  using MediaTrack::MediaTrack;
};

/**
 * @see https://html.spec.whatwg.org/multipage/media.html#videotrack
 */
class VideoTrack : public MediaTrack {
  DECLARE_TYPE_INFO(VideoTrack);

 public:
  using MediaTrack::MediaTrack;
};

class AudioTrackFactory : public BackingObjectFactory<AudioTrack> {
 public:
  AudioTrackFactory();
};

class VideoTrackFactory : public BackingObjectFactory<VideoTrack> {
 public:
  VideoTrackFactory();
};

}  // namespace mse
}  // namespace js
}  // namespace shaka

DEFINE_ENUM_MAPPING(shaka::media, MediaTrackKind) {
  AddMapping(Enum::Unknown, "");
  AddMapping(Enum::Alternative, "alternative");
  AddMapping(Enum::Captions, "captions");
  AddMapping(Enum::Descriptions, "descriptions");
  AddMapping(Enum::Main, "main");
  AddMapping(Enum::MainDesc, "main-desc");
  AddMapping(Enum::Sign, "sign");
  AddMapping(Enum::Subtitles, "subtitles");
  AddMapping(Enum::Translation, "translation");
  AddMapping(Enum::Commentary, "commentary");
}

#endif  // SHAKA_EMBEDDED_JS_MSE_MEDIA_TRACK_H_
