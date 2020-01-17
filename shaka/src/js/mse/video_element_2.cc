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

#include "src/js/mse/video_element_2.h"

#include "src/js/dom/document.h"
#include "src/util/macros.h"

namespace shaka {
namespace js {
namespace mse {

BEGIN_ALLOW_COMPLEX_STATICS
std::unordered_set<HTMLVideoElement*> HTMLVideoElement::g_video_elements_;
END_ALLOW_COMPLEX_STATICS

DEFINE_STRUCT_SPECIAL_METHODS_COPYABLE(VideoPlaybackQuality);

HTMLVideoElement::HTMLVideoElement(RefPtr<dom::Document> document,
                                   media::MediaPlayer* player)
    : HTMLMediaElement(document, "video", player) {
  g_video_elements_.insert(this);
}

// \cond Doxygen_Skip
HTMLVideoElement::~HTMLVideoElement() {
  g_video_elements_.erase(this);
}
// \endcond Doxygen_Skip

RefPtr<HTMLVideoElement> HTMLVideoElement::AnyVideoElement() {
  return *g_video_elements_.begin();
}

media::MediaPlayer* HTMLVideoElement::AnyMediaPlayer() {
  if (g_video_elements_.empty())
    return nullptr;
  return (*g_video_elements_.begin())->player_;
}

ExceptionOr<VideoPlaybackQuality> HTMLVideoElement::GetVideoPlaybackQuality()
    const {
  VideoPlaybackQuality ret;
  if (player_) {
    auto temp = player_->VideoPlaybackQuality();
    ret.totalVideoFrames = temp.total_video_frames;
    ret.droppedVideoFrames = temp.dropped_video_frames;
    ret.corruptedVideoFrames = temp.corrupted_video_frames;
  }
  return ret;
}


HTMLVideoElementFactory::HTMLVideoElementFactory() {
  AddMemberFunction("getVideoPlaybackQuality",
                    &HTMLVideoElement::GetVideoPlaybackQuality);
}

}  // namespace mse
}  // namespace js
}  // namespace shaka
