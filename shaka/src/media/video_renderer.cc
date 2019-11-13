// Copyright 2017 Google LLC
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

#include "src/media/video_renderer.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "src/media/stream.h"

namespace shaka {
namespace media {

namespace {

/** The minimum delay, in seconds, to delay between drawing frames. */
constexpr const double kMinDelay = 1.0 / 120;

/** The maximum delay, in seconds, to delay between drawing frames. */
constexpr const double kMaxDelay = 1.0 / 15;

}  // namespace

VideoRenderer::VideoRenderer(std::function<double()> get_time, Stream* stream)
    : mutex_("VideoRenderer"),
      stream_(stream),
      get_time_(std::move(get_time)),
      prev_time_(-1),
      is_seeking_(false) {}

VideoRenderer::~VideoRenderer() {}


std::shared_ptr<DecodedFrame> VideoRenderer::DrawFrame(int* dropped_frame_count,
                                                       bool* is_new_frame,
                                                       double* delay) {
  std::unique_lock<Mutex> lock(mutex_);

  // Discard any old frames, except when seeking.
  if (!is_seeking_ && prev_time_ >= 0)
    stream_->GetDecodedFrames()->Remove(0, prev_time_ - 0.2);

  // TODO: Could this usage cause a deadlock?  If a remove() is started after
  // ideal_frame is locked, the remove() will block and getting next_frame
  // will wait for remove().
  const double time = get_time_();
  // If we are seeking, use the previous frame so we display the same frame
  // while the seek is happening.
  auto ideal_frame =
      is_seeking_ && prev_time_ >= 0
          ? stream_->GetDecodedFrames()->GetFrame(prev_time_,
                                                  FrameLocation::Near)
          : stream_->GetDecodedFrames()->GetFrame(time, FrameLocation::Near);
  if (!ideal_frame)
    return nullptr;

  // TODO: Consider changing effective playback rate to speed up video when
  // behind.  This makes playback smoother at the cost of being more complicated
  // and sacrificing AV sync.

  auto next_frame = stream_->GetDecodedFrames()->GetFrame(ideal_frame->pts,
                                                          FrameLocation::After);
  const double total_delay = next_frame ? next_frame->pts - time : 0;
  *delay = std::max(std::min(total_delay, kMaxDelay), kMinDelay);

  *is_new_frame = prev_time_ != ideal_frame->pts;
  if (!is_seeking_) {
    if (prev_time_ >= 0) {
      *dropped_frame_count = stream_->GetDecodedFrames()->CountFramesBetween(
          prev_time_, ideal_frame->pts);
    }
    prev_time_ = ideal_frame->pts;
  }
  return ideal_frame;
}

void VideoRenderer::OnSeek() {
  std::unique_lock<Mutex> lock(mutex_);
  is_seeking_ = true;
}

void VideoRenderer::OnSeekDone() {
  std::unique_lock<Mutex> lock(mutex_);
  is_seeking_ = false;
  prev_time_ = -1;

  // Now that the seek is done, discard frames from the previous time while
  // keeping the newly decoded frames.  Don't discard too close to current time
  // since we might discard frames that were just decoded.
  const double time = get_time_();
  stream_->GetDecodedFrames()->Remove(0, time - 1);
  stream_->GetDecodedFrames()->Remove(time + 1, HUGE_VAL);
}

}  // namespace media
}  // namespace shaka
