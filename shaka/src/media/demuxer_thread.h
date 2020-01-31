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

#ifndef SHAKA_EMBEDDED_MEDIA_DEMUXER_THREAD_H_
#define SHAKA_EMBEDDED_MEDIA_DEMUXER_THREAD_H_

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include "shaka/media/demuxer.h"
#include "shaka/media/streams.h"
#include "src/debug/mutex.h"
#include "src/debug/thread.h"
#include "src/debug/thread_event.h"
#include "src/media/types.h"
#include "src/util/buffer_reader.h"
#include "src/util/macros.h"

namespace shaka {
namespace media {

/**
 * Handles the thread that demuxes input content.  This handles synchronizing
 * the threads and connecting the Demuxer to the Stream.
 *
 * All callbacks given to this object will be called on the event thread.
 */
class DemuxerThread {
 public:
  /**
   * Creates a new Demuxer instance that pushes to the given stream.
   * @param mime The full MIME type this will read from.
   * @param client A client interface object for events.
   * @param stream The stream to push frames to.
   */
  DemuxerThread(const std::string& mime, Demuxer::Client* client,
                ElementaryStream* stream);
  ~DemuxerThread();

  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(DemuxerThread);

  /** Stops the background thread and joins it. */
  void Stop();

  /**
   * Appends the given data to be demuxed.
   *
   * @param timestamp_offset The number of seconds to move the media timestamps
   *   forward.
   * @param window_start The time (in seconds) to start the append window.  Any
   *   frames outside the append window are ignored.
   * @param window_end The time (in seconds) to end the append window.
   * @param data The data pointer; it must remain alive until a call to either
   *     on_complete or on_error.
   * @param data_size The number of bytes in |data|.
   * @param on_complete The callback to invoke once the append completes.
   */
  void AppendData(double timestamp_offset, double window_start,
                  double window_end, const uint8_t* data, size_t data_size,
                  std::function<void(bool)> on_complete);

 private:
  void ThreadMain();
  void CallOnComplete(bool success);

  Mutex mutex_;
  std::unique_ptr<Demuxer> demuxer_;
  ThreadEvent<void> new_data_;
  std::function<void(bool)> on_complete_;
  Demuxer::Client* client_;
  std::string mime_;
  std::atomic<bool> shutdown_;
  const uint8_t* data_;
  size_t data_size_;
  double timestamp_offset_;
  double window_start_;
  double window_end_;
  bool need_key_frame_;

  ElementaryStream* stream_;

  // Should be last so the thread starts after all the fields are initialized.
  Thread thread_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_DEMUXER_THREAD_H_
