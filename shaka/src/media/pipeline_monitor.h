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

#ifndef SHAKA_EMBEDDED_MEDIA_PIPELINE_MONITOR_H_
#define SHAKA_EMBEDDED_MEDIA_PIPELINE_MONITOR_H_

#include <functional>

#include "shaka/media/media_player.h"
#include "src/debug/mutex.h"
#include "src/debug/thread.h"
#include "src/debug/thread_event.h"
#include "src/media/pipeline_manager.h"
#include "src/media/types.h"
#include "src/util/clock.h"

namespace shaka {
namespace media {

/**
 * This manages a thread that monitors the media pipeline and updates the state
 * based on the currently buffered content.  This also handles transitioning to
 * ended.
 */
class PipelineMonitor {
 public:
  PipelineMonitor(std::function<BufferedRanges()> get_buffered,
                  std::function<BufferedRanges()> get_decoded,
                  std::function<void(VideoReadyState)> ready_state_changed,
                  const util::Clock* clock, PipelineManager* pipeline);
  ~PipelineMonitor();

  /** Starts monitoring the current state. */
  void Start();

  /** Stops monitoring and waits for a call to start. */
  void Stop();

 private:
  void ThreadMain();

  void ChangeReadyState(VideoReadyState new_state);

  Mutex mutex_;
  ThreadEvent<void> start_;

  const std::function<BufferedRanges()> get_buffered_;
  const std::function<BufferedRanges()> get_decoded_;
  const std::function<void(VideoReadyState)> ready_state_changed_;
  const util::Clock* const clock_;
  PipelineManager* const pipeline_;
  bool shutdown_;
  bool running_;
  VideoReadyState ready_state_;

  Thread thread_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_PIPELINE_MONITOR_H_
