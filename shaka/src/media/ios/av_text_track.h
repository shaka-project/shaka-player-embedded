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

#ifndef SHAKA_EMBEDDED_MEDIA_IOS_AV_TEXT_TRACK_H_
#define SHAKA_EMBEDDED_MEDIA_IOS_AV_TEXT_TRACK_H_

#import <AVFoundation/AVFoundation.h>

#include "shaka/media/text_track.h"
#include "src/media/ios/av_media_track.h"

namespace shaka {
namespace media {
namespace ios {

/**
 * Defines a subclass of TextTrack that handles selecting the track.  This
 * allows selecting the resulting track for playback.  This doesn't track
 * individual cues since those aren't exposed through AVFoundation.
 *
 * This subclasses AvMediaTrack to avoid code duplication for handling of the
 * iOS track.
 */
class AvTextTrack : AvMediaTrack, public TextTrack {
 public:
  AvTextTrack(AVPlayerItem *item, AVMediaSelectionGroup *group,
               AVMediaSelectionOption *option);
  ~AvTextTrack() override;

  TextTrackMode mode() const override;
  void SetMode(TextTrackMode mode) override;
};

}  // namespace ios
}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_IOS_AV_TEXT_TRACK_H_
