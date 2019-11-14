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

#ifndef SHAKA_EMBEDDED_MEDIA_AUDIO_RENDERER_H_
#define SHAKA_EMBEDDED_MEDIA_AUDIO_RENDERER_H_

#include <SDL2/SDL.h>

#include <functional>

#include "shaka/media/frames.h"
#include "shaka/media/streams.h"
#include "src/debug/mutex.h"
#include "src/debug/thread.h"
#include "src/debug/thread_event.h"
#include "src/media/renderer.h"
#include "src/util/macros.h"

struct SwrContext;

namespace shaka {
namespace media {

/**
 * Defines a renderer that draws audio frames to the audio device.
 */
class AudioRenderer : public Renderer {
 public:
  AudioRenderer(std::function<double()> get_time,
                std::function<double()> get_playback_rate,
                DecodedStream* stream);
  ~AudioRenderer() override;

  // TODO: Allow setting different output device.

  NON_COPYABLE_OR_MOVABLE_TYPE(AudioRenderer);

  void OnSeek() override;
  void OnSeekDone() override;

  /** Sets the volume of the audio. */
  void SetVolume(double volume);

 private:
  void ThreadMain();
  bool InitDevice(const DecodedFrame* frame);

  static void OnAudioCallback(void*, uint8_t*, int);
  void AudioCallback(uint8_t* data, int size);

  const std::function<double()> get_time_;
  const std::function<double()> get_playback_rate_;
  DecodedStream* const stream_;

  mutable Mutex mutex_;
  ThreadEvent<void> on_reset_;
  SDL_AudioSpec audio_spec_;
  SDL_AudioSpec obtained_audio_spec_;
  SDL_AudioDeviceID audio_device_;
  SwrContext* swr_ctx_;
  double cur_time_;
  double volume_;
  bool shutdown_ : 1;
  bool need_reset_ : 1;
  bool is_seeking_ : 1;

  Thread thread_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_AUDIO_RENDERER_H_
