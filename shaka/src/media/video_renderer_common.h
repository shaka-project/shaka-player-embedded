// Copyright 2019 Google LLC
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

#ifndef SHAKA_EMBEDDED_MEDIA_VIDEO_RENDERER_COMMON_H_
#define SHAKA_EMBEDDED_MEDIA_VIDEO_RENDERER_COMMON_H_

#include <atomic>
#include <memory>

#include "shaka/media/media_player.h"
#include "shaka/media/renderer.h"
#include "src/debug/mutex.h"

namespace shaka {
namespace media {

/**
 * Holds common code between our VideoRenderer types.  This handles selecting
 * the current frame, tracking frame counts, and changing fields.
 */
class VideoRendererCommon : public VideoRenderer, MediaPlayer::Client {
 public:
  VideoRendererCommon();
  ~VideoRendererCommon() override;

  VideoFillMode fill_mode() const;

  /**
   * Gets the current frame and updates frame statistics.
   * @param frame [OUT] Where to put the resulting frame.
   * @return The delay until the frame after this one.
   */
  double GetCurrentFrame(std::shared_ptr<DecodedFrame>* frame);


  void SetPlayer(const MediaPlayer* player) override;
  void Attach(const DecodedStream* stream) override;
  void Detach() override;

  struct VideoPlaybackQuality VideoPlaybackQuality() const override;
  bool SetVideoFillMode(VideoFillMode mode) override;

 private:
  void OnSeeking() override;

  mutable Mutex mutex_;

  const MediaPlayer* player_;
  const DecodedStream* input_;
  struct VideoPlaybackQuality quality_;
  std::atomic<VideoFillMode> fill_mode_;
  double prev_time_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_VIDEO_RENDERER_COMMON_H_
