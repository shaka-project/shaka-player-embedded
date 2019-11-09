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

#include "src/media/frame_buffer.h"

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
    if (range.start <= start_time + FrameBuffer::kMaxGapSize &&
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
    std::function<void(MediaReadyState)> ready_state_changed,
    const util::Clock* clock, PipelineManager* pipeline)
    : get_buffered_(std::move(get_buffered)),
      get_decoded_(std::move(get_decoded)),
      ready_state_changed_(std::move(ready_state_changed)),
      clock_(clock),
      pipeline_(pipeline),
      shutdown_(false),
      ready_state_(HAVE_NOTHING),
      thread_("PipelineMonitor",
              std::bind(&PipelineMonitor::ThreadMain, this)) {}

PipelineMonitor::~PipelineMonitor() {
  CHECK(!thread_.joinable()) << "Need to call Stop() before destroying";
}

void PipelineMonitor::Stop() {
  shutdown_.store(true, std::memory_order_release);
  thread_.join();
}

bool AtEnd(const BufferedRanges& ranges, double time, double duration) {
  if (std::isnan(duration)) {
    return false;
  }

  const double relative_time = time - duration;

  if (relative_time >= 0) {
    return true;
  }

  // With presentation time tracked with the help of audio device/data a
  // possible scenario is that audio track is effectively shorter than
  // advertised duration, or implicit duration defined by video track. In such
  // circumstances playback time is unable to reach duration exactly. Being
  // |kNearEndRangeThreshold| close to end of playback, we assume end of
  // presentation on reaching end time of last buffered range (which itself is
  // an intersection of ranges of tracks). Threshold of |kAssumeEndThreshold|
  // compensates for possible discrepancy of external clock processing last
  // packet with respective update of presentation time.
  //
  // To be even more precise and ensure exactly last video frame is displayed
  // from available data, we could possibly notify external clock of expected
  // duration and allow media time reporting past its data derived from audio
  // track. In this case audio playback subsystem having no real data but with
  // known duration would be able to use audio hardware defined clock to
  // continue advancing time up to duration value and take playback exactly to
  // duration time.
  static constexpr const double kNearEndRangeThresholdS = 0.500;
  static constexpr const double kAssumeEndThresholdS = 0.025;

  if (relative_time < -kNearEndRangeThresholdS) {
    return false;
  }

  if (ranges.empty()) {
    return true;
  }

  return time + kAssumeEndThresholdS >= ranges.back().end;
}

void PipelineMonitor::ThreadMain() {
  while (!shutdown_.load(std::memory_order_acquire)) {
    const BufferedRanges buffered = get_buffered_();
    const BufferedRanges decoded = get_decoded_();
    const double time = pipeline_->GetCurrentTime();
    const double duration = pipeline_->GetDuration();
    const bool can_play = CanPlay(buffered, time, duration);
    if (AtEnd(decoded, time, duration)) {
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
      ChangeReadyState(HAVE_NOTHING);
    } else if (can_play) {
      ChangeReadyState(HAVE_FUTURE_DATA);
    } else if (IsBufferedUntil(buffered, time, time, duration)) {
      ChangeReadyState(HAVE_CURRENT_DATA);
    } else {
      ChangeReadyState(HAVE_METADATA);
    }

    clock_->SleepSeconds(0.01);
  }
}

void PipelineMonitor::ChangeReadyState(MediaReadyState new_state) {
  if (ready_state_ != new_state) {
    ready_state_ = new_state;
    ready_state_changed_(new_state);
  }
}

}  // namespace media
}  // namespace shaka
