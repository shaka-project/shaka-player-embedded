// Copyright 2020 Google LLC
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

#ifndef SHAKA_EMBEDDED_MEDIA_APPLE_AUDIO_RENDERER_H_
#define SHAKA_EMBEDDED_MEDIA_APPLE_AUDIO_RENDERER_H_

#include <memory>
#include <string>

#include "../macros.h"
#include "renderer.h"

namespace shaka {
namespace media {

/**
 * Defines an audio renderer that renders frames using AudioToolbox.
 *
 * @ingroup media
 */
class SHAKA_EXPORT AppleAudioRenderer final : public AudioRenderer {
 public:
  AppleAudioRenderer();
  ~AppleAudioRenderer() override;

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

#endif  // SHAKA_EMBEDDED_MEDIA_APPLE_AUDIO_RENDERER_H_
