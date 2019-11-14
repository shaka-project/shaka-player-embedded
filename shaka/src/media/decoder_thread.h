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

#include "shaka/media/decoder.h"
#include "shaka/media/streams.h"
#include "src/debug/mutex.h"
#include "src/debug/thread.h"
#include "src/debug/thread_event.h"
#include "src/util/macros.h"

namespace shaka {

namespace eme {
class Implementation;
}  // namespace eme

namespace media {

/**
 * Handles the thread that decodes input content.  This handles synchronizing
 * the threads and connecting the Decoder to the Stream.
 */
class DecoderThread {
 public:
  class Client {
   public:
    virtual ~Client() {}

    // These should have the same names as MediaPlayer to ensure we can subclass
    // both.
    virtual double CurrentTime() const = 0;
    virtual double Duration() const = 0;
    virtual void OnWaitingForKey() = 0;

    virtual void OnSeekDone() = 0;
    virtual void OnError() = 0;
  };

  /**
   * @param client A client object for callback events.
   * @param output The object to put decoded frames into.
   */
  DecoderThread(Client* client, DecodedStream* output);
  ~DecoderThread();

  NON_COPYABLE_OR_MOVABLE_TYPE(DecoderThread);

  /** Starts decoding frames from the given stream. */
  void Attach(const ElementaryStream* input);

  /** Stops decoding frames from the current stream. */
  void Detach();

  /**
   * Called when the video seeks.  This should reset any internal data and start
   * over decoding.
   */
  void OnSeek();

  void SetCdm(eme::Implementation* cdm);

  /** Sets the decoder used to decode frames. */
  void SetDecoder(Decoder* decoder);

 private:
  void ThreadMain();

  Mutex mutex_;
  ThreadEvent<void> signal_;

  Client* const client_;
  const ElementaryStream* input_;
  DecodedStream* const output_;
  Decoder* decoder_;

  eme::Implementation* cdm_;
  double last_frame_time_;
  bool shutdown_;
  bool is_seeking_;
  bool did_flush_;
  bool raised_waiting_event_;

  Thread thread_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_DECODER_THREAD_H_
