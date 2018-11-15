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

#include "src/media/pipeline_manager.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "src/util/utils.h"

namespace shaka {
namespace media {

PipelineManager::PipelineManager(
    std::function<void(PipelineStatus)> on_status_changed,
    std::function<void()> on_seek, const util::Clock* clock)
    : mutex_("PipelineManager"),
      on_status_changed_(std::move(on_status_changed)),
      on_seek_(std::move(on_seek)),
      clock_(clock),
      status_(PipelineStatus::Initializing),
      prev_media_time_(0),
      prev_wall_time_(clock->GetMonotonicTime()),
      playback_rate_(1),
      duration_(NAN),
      autoplay_(false) {}

PipelineManager::~PipelineManager() {}

void PipelineManager::DoneInitializing() {
  PipelineStatus new_status;
  {
    std::unique_lock<SharedMutex> lock(mutex_);
    if (status_ == PipelineStatus::Errored)
      return;
    DCHECK_EQ(status_, PipelineStatus::Initializing);
    if (autoplay_) {
      new_status = status_ = PipelineStatus::Stalled;
    } else {
      new_status = status_ = PipelineStatus::Paused;
    }
  }
  on_status_changed_(new_status);
}

PipelineStatus PipelineManager::GetPipelineStatus() const {
  util::shared_lock<SharedMutex> lock(mutex_);
  return status_;
}

double PipelineManager::GetDuration() const {
  util::shared_lock<SharedMutex> lock(mutex_);
  return duration_;
}

void PipelineManager::SetDuration(double duration) {
  PipelineStatus new_status = PipelineStatus::Initializing;
  {
    std::unique_lock<SharedMutex> lock(mutex_);
    duration_ = duration;

    // Seek to duration if current time is past the new duration.
    const uint64_t wall_time = clock_->GetMonotonicTime();
    if (!std::isnan(duration) && GetTimeFor(wall_time) > duration) {
      {
        util::Unlocker<SharedMutex> unlock(&lock);
        on_seek_();
      }

      prev_media_time_ = duration;
      prev_wall_time_ = wall_time;
      if (status_ == PipelineStatus::Playing ||
          status_ == PipelineStatus::Stalled) {
        new_status = status_ = PipelineStatus::SeekingPlay;
      } else if (status_ == PipelineStatus::Paused ||
                 status_ == PipelineStatus::Ended) {
        new_status = status_ = PipelineStatus::SeekingPause;
      }
    }
  }
  if (new_status != PipelineStatus::Initializing)
    on_status_changed_(new_status);
}

double PipelineManager::GetCurrentTime() const {
  util::shared_lock<SharedMutex> lock(mutex_);
  return GetTimeFor(clock_->GetMonotonicTime());
}

void PipelineManager::SetCurrentTime(double time) {
  PipelineStatus new_status = PipelineStatus::Initializing;
  {
    std::unique_lock<SharedMutex> lock(mutex_);
    if (status_ != PipelineStatus::Initializing &&
        status_ != PipelineStatus::Errored) {
      {
        util::Unlocker<SharedMutex> unlock(&lock);
        on_seek_();
      }

      prev_media_time_ =
          std::isnan(duration_) ? time : std::min(duration_, time);
      prev_wall_time_ = clock_->GetMonotonicTime();
      switch (status_) {
        case PipelineStatus::Playing:
        case PipelineStatus::Stalled:
        case PipelineStatus::SeekingPlay:
          new_status = status_ = PipelineStatus::SeekingPlay;
          break;
        case PipelineStatus::Paused:
        case PipelineStatus::Ended:
        case PipelineStatus::SeekingPause:
          new_status = status_ = PipelineStatus::SeekingPause;
          break;
        default:  // Ignore remaining enum values.
          break;
      }
    }
  }
  if (new_status != PipelineStatus::Initializing)
    on_status_changed_(new_status);
}

double PipelineManager::GetPlaybackRate() const {
  util::shared_lock<SharedMutex> lock(mutex_);
  return playback_rate_;
}

void PipelineManager::SetPlaybackRate(double rate) {
  std::unique_lock<SharedMutex> lock(mutex_);
  SyncPoint();
  playback_rate_ = rate;
}

void PipelineManager::Play() {
  PipelineStatus new_status = PipelineStatus::Initializing;
  {
    std::unique_lock<SharedMutex> lock(mutex_);
    SyncPoint();
    if (status_ == PipelineStatus::Paused) {
      // Assume we are stalled; we will transition to Playing quickly if not.
      new_status = status_ = PipelineStatus::Stalled;
    } else if (status_ == PipelineStatus::Ended) {
      {
        util::Unlocker<SharedMutex> unlock(&lock);
        on_seek_();
      }

      prev_media_time_ = 0;
      new_status = status_ = PipelineStatus::SeekingPlay;
    } else if (status_ == PipelineStatus::SeekingPause) {
      new_status = status_ = PipelineStatus::SeekingPlay;
    } else if (status_ == PipelineStatus::Initializing) {
      autoplay_ = true;
    }
  }
  if (new_status != PipelineStatus::Initializing)
    on_status_changed_(new_status);
}

void PipelineManager::Pause() {
  PipelineStatus new_status = PipelineStatus::Initializing;
  {
    std::unique_lock<SharedMutex> lock(mutex_);
    SyncPoint();
    if (status_ == PipelineStatus::Playing ||
        status_ == PipelineStatus::Stalled) {
      new_status = status_ = PipelineStatus::Paused;
    } else if (status_ == PipelineStatus::SeekingPlay) {
      new_status = status_ = PipelineStatus::SeekingPause;
    } else if (status_ == PipelineStatus::Initializing) {
      autoplay_ = false;
    }
  }
  if (new_status != PipelineStatus::Initializing)
    on_status_changed_(new_status);
}

void PipelineManager::Stalled() {
  bool status_changed = false;
  {
    std::unique_lock<SharedMutex> lock(mutex_);
    if (status_ == PipelineStatus::Playing) {
      SyncPoint();
      status_ = PipelineStatus::Stalled;
      status_changed = true;
    }
  }
  if (status_changed)
    on_status_changed_(PipelineStatus::Stalled);
}

void PipelineManager::CanPlay() {
  PipelineStatus new_status = PipelineStatus::Initializing;
  {
    std::unique_lock<SharedMutex> lock(mutex_);
    SyncPoint();
    if (status_ == PipelineStatus::Stalled ||
        status_ == PipelineStatus::SeekingPlay) {
      new_status = status_ = PipelineStatus::Playing;
    } else if (status_ == PipelineStatus::SeekingPause) {
      new_status = status_ = PipelineStatus::Paused;
    }
  }
  if (new_status != PipelineStatus::Initializing)
    on_status_changed_(new_status);
}

void PipelineManager::OnEnded() {
  PipelineStatus new_status = PipelineStatus::Initializing;
  {
    std::unique_lock<SharedMutex> lock(mutex_);
    if (status_ != PipelineStatus::Ended &&
        status_ != PipelineStatus::Errored) {
      const uint64_t wall_time = clock_->GetMonotonicTime();
      DCHECK(!std::isnan(duration_));
      prev_wall_time_ = wall_time;
      prev_media_time_ = duration_;
      new_status = status_ = PipelineStatus::Ended;
    }
  }
  if (new_status != PipelineStatus::Initializing)
    on_status_changed_(new_status);
}

void PipelineManager::OnError() {
  bool fire_event = false;
  {
    std::unique_lock<SharedMutex> lock(mutex_);
    if (status_ != PipelineStatus::Errored) {
      SyncPoint();
      status_ = PipelineStatus::Errored;
      fire_event = true;
    }
  }
  if (fire_event)
    on_status_changed_(PipelineStatus::Errored);
}

double PipelineManager::GetTimeFor(uint64_t wall_time) const {
  if (status_ != PipelineStatus::Playing)
    return prev_media_time_;

  const uint64_t wall_diff = wall_time - prev_wall_time_;
  const double time = prev_media_time_ + (wall_diff * playback_rate_ / 1000.0);
  return std::isnan(duration_) ? time : std::min(duration_, time);
}

void PipelineManager::SyncPoint() {
  const uint64_t wall_time = clock_->GetMonotonicTime();
  prev_media_time_ = GetTimeFor(wall_time);
  prev_wall_time_ = wall_time;
}

}  // namespace media
}  // namespace shaka
