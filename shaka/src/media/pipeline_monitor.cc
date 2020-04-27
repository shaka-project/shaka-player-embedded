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

#include "src/media/pipeline_monitor.h"

#include <utility>

#include "shaka/media/streams.h"
#include "src/util/utils.h"

namespace shaka {
namespace media {

namespace {

/** The number of seconds of content needed to be able to play forward. */
constexpr const double kNeedForPlay = 0.3;
/** The number of seconds difference to assume we are at the end. */
constexpr const double kEpsilon = 0.1;

bool IsBufferedUntil(const BufferedRanges& ranges, double start_time,
                     double end_time, double duration) {
  for (auto& range : ranges) {
    if (range.start <= start_time + StreamBase::kMaxGapSize &&
        (range.end >= end_time || end_time + kEpsilon >= duration)) {
      return true;
    }
  }
  return false;
}

bool CanPlay(const BufferedRanges& ranges, double time, double duration) {
  return IsBufferedUntil(ranges, time, time + kNeedForPlay, duration);
}

}  // namespace

PipelineMonitor::PipelineMonitor(
    std::function<BufferedRanges()> get_buffered,
    std::function<BufferedRanges()> get_decoded,
    std::function<void(VideoReadyState)> ready_state_changed,
    const util::Clock* clock, PipelineManager* pipeline)
    : mutex_("PipelineMonitor"),
      start_("PipelineMonitor::Start"),
      get_buffered_(std::move(get_buffered)),
      get_decoded_(std::move(get_decoded)),
      ready_state_changed_(std::move(ready_state_changed)),
      clock_(clock),
      pipeline_(pipeline),
      shutdown_(false),
      running_(false),
      ready_state_(VideoReadyState::HaveNothing),
      thread_("PipelineMonitor",
              std::bind(&PipelineMonitor::ThreadMain, this)) {}

PipelineMonitor::~PipelineMonitor() {
  {
    std::unique_lock<Mutex> lock(mutex_);
    shutdown_ = true;
    start_.SignalAllIfNotSet();
  }
  thread_.join();
}

void PipelineMonitor::Start() {
  std::unique_lock<Mutex> lock(mutex_);
  ready_state_ = VideoReadyState::HaveNothing;
  running_ = true;
  start_.SignalAllIfNotSet();
}

void PipelineMonitor::Stop() {
  std::unique_lock<Mutex> lock(mutex_);
  running_ = false;
}

void PipelineMonitor::ThreadMain() {
  std::unique_lock<Mutex> lock(mutex_);
  while (!shutdown_) {
    if (!running_) {
      start_.ResetAndWaitWhileUnlocked(lock);
      continue;
    }

    const BufferedRanges buffered = get_buffered_();
    const BufferedRanges decoded = get_decoded_();
    const double time = pipeline_->GetCurrentTime();
    const double duration = pipeline_->GetDuration();
    const auto state = pipeline_->GetPlaybackState();
    // Don't move playhead until we have decoded at the current time.  This
    // ensures we stop for decryption errors and that we don't blindly move
    // forward without the correct frames.
    // If we're already playing, keep playing until the end of the buffered
    // range; otherwise wait until we have buffered some amount ahead of the
    // playhead.
    const bool is_playing = state == VideoPlaybackState::Playing;
    const bool has_current_frame =
        IsBufferedUntil(decoded, time, time, duration);
    const bool can_start =
        CanPlay(buffered, time, duration) && has_current_frame;
    const bool can_play = is_playing ? has_current_frame : can_start;
    if (time >= duration) {
      pipeline_->OnEnded();
    } else if (can_play) {
      pipeline_->CanPlay();
    } else {
      pipeline_->Buffering();
    }

    if (state == VideoPlaybackState::Initializing) {
      ChangeReadyState(VideoReadyState::HaveNothing);
    } else if (can_play) {
      ChangeReadyState(VideoReadyState::HaveFutureData);
    } else if (has_current_frame) {
      ChangeReadyState(VideoReadyState::HaveCurrentData);
    } else {
      ChangeReadyState(VideoReadyState::HaveMetadata);
    }

    util::Unlocker<Mutex> unlock(&lock);
    clock_->SleepSeconds(0.01);
  }
}

void PipelineMonitor::ChangeReadyState(VideoReadyState new_state) {
  if (ready_state_ != new_state) {
    ready_state_ = new_state;
    ready_state_changed_(new_state);
  }
}

}  // namespace media
}  // namespace shaka
