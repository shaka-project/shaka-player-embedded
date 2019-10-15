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

#include "shaka/media/sdl_video_renderer.h"

namespace shaka {
namespace media {

class SdlManualVideoRenderer::Impl {};

SdlManualVideoRenderer::SdlManualVideoRenderer(SDL_Renderer* renderer) {}
SdlManualVideoRenderer::~SdlManualVideoRenderer() {}

void SdlManualVideoRenderer::SetRenderer(SDL_Renderer* renderer) {}
SDL_Renderer* SdlManualVideoRenderer::GetRenderer() const {
  return nullptr;
}

double SdlManualVideoRenderer::Render(SDL_Rect* region) {
  return 0;
}

void SdlManualVideoRenderer::OnSeek() {}

void SdlManualVideoRenderer::SetPlayer(const MediaPlayer* player) {}
void SdlManualVideoRenderer::Attach(const DecodedStream* stream) {}
void SdlManualVideoRenderer::Detach() {}

VideoPlaybackQualityNew SdlManualVideoRenderer::VideoPlaybackQuality() const {
  return VideoPlaybackQualityNew();
}
bool SdlManualVideoRenderer::SetVideoFillMode(VideoFillMode mode) {
  return false;
}


class SdlThreadVideoRenderer::Impl {};

SdlThreadVideoRenderer::SdlThreadVideoRenderer(SDL_Renderer* renderer) {}
SdlThreadVideoRenderer::SdlThreadVideoRenderer(SDL_Renderer* renderer,
                                               SDL_Rect region) {}
SdlThreadVideoRenderer::~SdlThreadVideoRenderer() {}

}  // namespace media
}  // namespace shaka
