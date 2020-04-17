// Copyright 2019 Google LLC
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

#ifndef SHAKA_EMBEDDED_MEDIA_SDL_AUDIO_RENDERER_H_
#define SHAKA_EMBEDDED_MEDIA_SDL_AUDIO_RENDERER_H_

#include <memory>
#include <string>
#include <vector>

#include "../macros.h"
#include "renderer.h"

namespace shaka {
namespace media {

/**
 * Defines an audio renderer that renders frames to the given SDL audio device.
 *
 * @ingroup media
 */
class SHAKA_EXPORT SdlAudioRenderer final : public AudioRenderer {
 public:
  /**
   * Creates a new audio renderer that plays using the given SDL audio device.
   * Can pass the empty string to use the default device.
   */
  SdlAudioRenderer(const std::string& device_name);
  ~SdlAudioRenderer() override;

  /**
   * Returns a vector containing all possible audio devices.  Other values may
   * be valid based on the system.  See SDL documentation for more info.
   */
  static std::vector<std::string> ListDevices();

  void SetPlayer(const MediaPlayer* player) override;
  void Attach(const DecodedStream* stream) override;
  void Detach() override;

  double Volume() const override;
  void SetVolume(double volume) override;
  bool Muted() const override;
  void SetMuted(bool muted) override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_SDL_AUDIO_RENDERER_H_
