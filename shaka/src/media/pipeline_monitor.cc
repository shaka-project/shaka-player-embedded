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
    : get_buffered_(std::move(get_buffered)),
      get_decoded_(std::move(get_decoded)),
      ready_state_changed_(std::move(ready_state_changed)),
      clock_(clock),
      pipeline_(pipeline),
      shutdown_(false),
      ready_state_(VideoReadyState::HaveNothing),
      thread_("PipelineMonitor",
              std::bind(&PipelineMonitor::ThreadMain, this)) {}

PipelineMonitor::~PipelineMonitor() {
  CHECK(!thread_.joinable()) << "Need to call Stop() before destroying";
}

void PipelineMonitor::Stop() {
  shutdown_.store(true, std::memory_order_release);
  thread_.join();
}

void PipelineMonitor::ThreadMain() {
  while (!shutdown_.load(std::memory_order_acquire)) {
    const BufferedRanges buffered = get_buffered_();
    const BufferedRanges decoded = get_decoded_();
    const double time = pipeline_->GetCurrentTime();
    const double duration = pipeline_->GetDuration();
    const bool can_play = CanPlay(buffered, time, duration);
    if (time >= duration) {
      pipeline_->OnEnded();
    } else if (can_play && IsBufferedUntil(decoded, time, time, duration)) {
      // Don't move playhead until we have decoded at the current time.  This
      // ensures we stop for decryption errors and that we don't blindly move
      // forward without the correct frames.
      pipeline_->CanPlay();
    } else {
      pipeline_->Stalled();
    }

    if (pipeline_->GetPipelineStatus() == PipelineStatus::Initializing) {
      ChangeReadyState(VideoReadyState::HaveNothing);
    } else if (can_play) {
      ChangeReadyState(VideoReadyState::HaveFutureData);
    } else if (IsBufferedUntil(buffered, time, time, duration)) {
      ChangeReadyState(VideoReadyState::HaveCurrentData);
    } else {
      ChangeReadyState(VideoReadyState::HaveMetadata);
    }

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
