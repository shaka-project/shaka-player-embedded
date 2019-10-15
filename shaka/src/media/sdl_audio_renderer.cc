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

#include "shaka/media/sdl_audio_renderer.h"

namespace shaka {
namespace media {

class SdlAudioRenderer::Impl {};

SdlAudioRenderer::SdlAudioRenderer(const std::string& device_name) {}
SdlAudioRenderer::~SdlAudioRenderer() {}

void SdlAudioRenderer::OnSeek() {}

void SdlAudioRenderer::SetPlayer(const MediaPlayer* player) {}
void SdlAudioRenderer::Attach(const DecodedStream* stream) {}
void SdlAudioRenderer::Detach() {}

double SdlAudioRenderer::Volume() const {
  return 0;
}

void SdlAudioRenderer::SetVolume(double volume) {}

bool SdlAudioRenderer::Muted() const {
  return false;
}

void SdlAudioRenderer::SetMuted(bool muted) {}

}  // namespace media
}  // namespace shaka
