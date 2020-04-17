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

#include "src/media/video_renderer_common.h"

#include <algorithm>

namespace shaka {
namespace media {

namespace {

/** The minimum delay, in seconds, to delay between drawing frames. */
constexpr const double kMinVideoDelay = 1.0 / 120;

/** The maximum delay, in seconds, to delay between drawing frames. */
constexpr const double kMaxVideoDelay = 1.0 / 15;

}  // namespace

VideoRendererCommon::VideoRendererCommon()
    : mutex_("VideoRendererCommon"),
      player_(nullptr),
      input_(nullptr),
      quality_(),
      fill_mode_(VideoFillMode::MaintainRatio),
      prev_time_(-1) {}

VideoRendererCommon::~VideoRendererCommon() {
  if (player_)
    player_->RemoveClient(this);
}

VideoFillMode VideoRendererCommon::fill_mode() const {
  return fill_mode_.load(std::memory_order_relaxed);
}

double VideoRendererCommon::GetCurrentFrame(
    std::shared_ptr<DecodedFrame>* frame) {
  std::unique_lock<Mutex> lock(mutex_);

  if (!player_ || !input_ ||
      player_->PlaybackState() == VideoPlaybackState::Seeking) {
    // If we are seeking, don't draw anything.  If the caller doesn't clear
    // the display, we will still show the frame before the seek.
    return kMinVideoDelay;
  }

  const double time = player_->CurrentTime();
  auto ideal_frame = input_->GetFrame(time, FrameLocation::Near);
  if (!ideal_frame)
    return kMinVideoDelay;

  // TODO: Consider changing effective playback rate to speed up video when
  // behind.  This makes playback smoother at the cost of being more
  // complicated and sacrificing AV sync.

  *frame = ideal_frame;

  auto next_frame = input_->GetFrame(ideal_frame->pts, FrameLocation::After);
  const double total_delay = next_frame ? next_frame->pts - time : 0;
  const double delay =
      std::max(std::min(total_delay, kMaxVideoDelay), kMinVideoDelay);

  if (prev_time_ >= 0) {
    const size_t count =
        input_->CountFramesBetween(prev_time_, ideal_frame->pts);
    quality_.dropped_video_frames += count;
    quality_.total_video_frames += count;
    if (ideal_frame->pts != prev_time_)
      quality_.total_video_frames++;
  } else {
    quality_.total_video_frames++;
  }
  prev_time_ = ideal_frame->pts;

  return delay;
}

void VideoRendererCommon::OnSeeking() {
  std::unique_lock<Mutex> lock(mutex_);
  prev_time_ = -1;
}

void VideoRendererCommon::SetPlayer(const MediaPlayer* player) {
  std::unique_lock<Mutex> lock(mutex_);
  if (player_)
    player_->RemoveClient(this);

  player_ = player;
  if (player)
    player->AddClient(this);
}

void VideoRendererCommon::Attach(const DecodedStream* stream) {
  std::unique_lock<Mutex> lock(mutex_);
  input_ = stream;
}

void VideoRendererCommon::Detach() {
  std::unique_lock<Mutex> lock(mutex_);
  input_ = nullptr;
}

VideoPlaybackQuality VideoRendererCommon::VideoPlaybackQuality() const {
  std::unique_lock<Mutex> lock(mutex_);
  return quality_;
}

bool VideoRendererCommon::SetVideoFillMode(VideoFillMode mode) {
  fill_mode_.store(mode, std::memory_order_relaxed);
  return true;
}

}  // namespace media
}  // namespace shaka
