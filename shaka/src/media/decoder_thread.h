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

#ifndef SHAKA_EMBEDDED_MEDIA_DECODER_THREAD_H_
#define SHAKA_EMBEDDED_MEDIA_DECODER_THREAD_H_

#include <atomic>
#include <functional>

#include "src/debug/thread.h"
#include "src/media/types.h"
#include "src/util/macros.h"

namespace shaka {

namespace eme {
class Implementation;
}  // namespace eme

namespace media {

class MediaProcessor;
class PipelineManager;
class Stream;


/**
 * Handles the thread that decodes input content.  This handles synchronizing
 * the threads and connecting the decoder part of MediaProcessor to the Stream.
 */
class DecoderThread {
 public:
  /**
   * @param get_time A callback to get the current playhead time.  This will be
   *   called from the background thread.
   * @param seek_done A callback for when a frame has been decoded after a seek.
   * @param on_waiting_for_key A callback for when the decoder is waiting for
   *   an encryption key.
   * @param on_error A callback for when there is a decoder error.
   * @param processor The processor that will process the media.
   * @param pipeline The pipeline that is used to determine the range of media.
   * @param stream The stream to pull frames from.
   */
  DecoderThread(std::function<double()> get_time,
                std::function<void()> seek_done,
                std::function<void()> on_waiting_for_key,
                std::function<void(Status)> on_error,
                MediaProcessor* processor,
                PipelineManager* pipeline,
                Stream* stream);
  ~DecoderThread();

  NON_COPYABLE_OR_MOVABLE_TYPE(DecoderThread);

  /** Stops the background thread and joins it. */
  void Stop();

  /**
   * Called when the video seeks.  This should reset any internal data and start
   * over decoding.
   */
  void OnSeek();

  void SetCdm(eme::Implementation* cdm);

 private:
  void ThreadMain();

  MediaProcessor* processor_;
  PipelineManager* pipeline_;
  Stream* stream_;

  std::function<double()> get_time_;
  std::function<void()> seek_done_;
  std::function<void()> on_waiting_for_key_;
  std::function<void(Status)> on_error_;
  std::atomic<eme::Implementation*> cdm_;
  std::atomic<bool> shutdown_;
  std::atomic<bool> is_seeking_;
  std::atomic<bool> did_flush_;
  std::atomic<double> last_frame_time_;
  bool raised_waiting_event_ = false;

  Thread thread_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_DECODER_THREAD_H_
