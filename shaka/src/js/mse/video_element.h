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

#ifndef SHAKA_EMBEDDED_JS_MSE_VIDEO_ELEMENT_H_
#define SHAKA_EMBEDDED_JS_MSE_VIDEO_ELEMENT_H_

#include <unordered_set>

#include "shaka/media/media_player.h"
#include "src/core/ref_ptr.h"
#include "src/js/mse/media_element.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/exception_or.h"
#include "src/mapping/struct.h"

namespace shaka {
namespace js {
namespace mse {

struct VideoPlaybackQuality : Struct {
  DECLARE_STRUCT_SPECIAL_METHODS_COPYABLE(VideoPlaybackQuality);

  ADD_DICT_FIELD(creationTime, double);
  ADD_DICT_FIELD(totalVideoFrames, uint64_t);
  ADD_DICT_FIELD(droppedVideoFrames, uint64_t);
  ADD_DICT_FIELD(corruptedVideoFrames, uint64_t);
};

class HTMLVideoElement : public HTMLMediaElement {
  DECLARE_TYPE_INFO(HTMLVideoElement);

 public:
  HTMLVideoElement(RefPtr<dom::Document> document,
                   media::MediaPlayer* player);

  static RefPtr<HTMLVideoElement> AnyVideoElement();
  static media::MediaPlayer* AnyMediaPlayer();

  ExceptionOr<VideoPlaybackQuality> GetVideoPlaybackQuality() const;

 private:
  static std::unordered_set<HTMLVideoElement*> g_video_elements_;
};

class HTMLVideoElementFactory
    : public BackingObjectFactory<HTMLVideoElement, HTMLMediaElement> {
 public:
  HTMLVideoElementFactory();
};

}  // namespace mse
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_MSE_VIDEO_ELEMENT_H_
