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

#include "src/debug/mutex.h"
#include "src/debug/thread.h"
#include "src/debug/thread_event.h"
#include "src/media/types.h"
#include "src/util/buffer_reader.h"
#include "src/util/macros.h"

namespace shaka {
namespace media {

class MediaProcessor;
class Stream;


/**
 * Handles the thread that demuxes input content.  This handles synchronizing
 * the threads and connecting the demuxer part of MediaProcessor to the Stream.
 *
 * All callbacks given to this object will be called on the event thread.
 */
class DemuxerThread {
 public:
  /**
   * Creates a new Demuxer instance that pushes to the given stream.
   * @param on_load_meta A callback to be invoked once we have loaded metadata.
   * @param processor The object that will process the input media.
   * @param stream The stream to push frames to.
   */
  DemuxerThread(std::function<void()> on_load_meta, MediaProcessor* processor,
                Stream* stream);
  ~DemuxerThread();

  NON_COPYABLE_OR_MOVABLE_TYPE(DemuxerThread);

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
                  std::function<void(Status)> on_complete);

 private:
  /**
   * Called by the MediaProcessor to read data.  This should fill |*data| with
   * the read data.
   * @param data Where to put the data that was read.
   * @param data_size The number of bytes in |data|.
   * @return The number of bytes read, or 0 on EOF.
   */
  size_t OnRead(uint8_t* data, size_t data_size);
  void OnResetRead();
  void ThreadMain();
  void CallOnComplete(Status status);

  Mutex mutex_;
  ThreadEvent<void> new_data_;
  std::function<void(Status)> on_complete_;
  std::function<void()> on_load_meta_;
  std::atomic<bool> shutdown_;
  util::BufferReader input_;
  const uint8_t* cur_data_;
  size_t cur_size_;
  double window_start_;
  double window_end_;
  bool need_key_frame_;

  MediaProcessor* processor_;
  Stream* stream_;

  // Should be last so the thread starts after all the fields are initialized.
  Thread thread_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_DEMUXER_THREAD_H_
