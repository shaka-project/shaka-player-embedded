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

#include <SDL2/SDL.h>

#include <atomic>

#include "shaka/optional.h"
#include "shaka/sdl_frame_drawer.h"
#include "shaka/utils.h"
#include "src/debug/mutex.h"
#include "src/debug/thread.h"
#include "src/media/video_renderer_common.h"
#include "src/util/clock.h"
#include "src/util/macros.h"
#include "src/util/utils.h"

namespace shaka {
namespace media {

class SdlManualVideoRenderer::Impl : public VideoRendererCommon {
 public:
  explicit Impl(SDL_Renderer* renderer)
      : mutex_("SdlManualVideoRenderer"), renderer_(renderer) {
    sdl_drawer_.SetRenderer(renderer);
  }

  void SetRenderer(SDL_Renderer* renderer) {
    std::unique_lock<Mutex> lock(mutex_);
    sdl_drawer_.SetRenderer(renderer);
    renderer_ = renderer;
  }

  SDL_Renderer* GetRenderer() const {
    std::unique_lock<Mutex> lock(mutex_);
    return renderer_;
  }

  double Render(const SDL_Rect* region) {
    std::shared_ptr<DecodedFrame> frame;
    const double delay = GetCurrentFrame(&frame);

    std::unique_lock<Mutex> lock(mutex_);
    if (frame && renderer_) {
      SDL_Texture* texture = sdl_drawer_.Draw(frame);
      if (texture) {
        ShakaRect region_shaka;
        if (region) {
          region_shaka = {region->x, region->y, region->w, region->h};
        } else {
          int w = frame->stream_info->width;
          int h = frame->stream_info->height;
          SDL_GetRendererOutputSize(renderer_, &w, &h);
          region_shaka.x = region_shaka.y = 0;
          region_shaka.w = w;
          region_shaka.h = h;
        }

        ShakaRect src, dest;
        ShakaRect frame_region = {0, 0, frame->stream_info->width,
                                  frame->stream_info->height};
        FitVideoToRegion(frame_region, region_shaka, fill_mode(), &src, &dest);
        SDL_Rect src_sdl = {src.x, src.y, src.w, src.h};
        SDL_Rect dest_sdl = {dest.x, dest.y, dest.w, dest.h};
        SDL_RenderCopy(renderer_, texture, &src_sdl, &dest_sdl);
      }
    }

    return delay;
  }

 private:
  mutable Mutex mutex_;
  SdlFrameDrawer sdl_drawer_;
  SDL_Renderer* renderer_;
};

class SdlThreadVideoRenderer::Impl {
 public:
  Impl(SdlThreadVideoRenderer* renderer, const SDL_Rect* region)
      : renderer_(renderer),
        region_(region ? optional<SDL_Rect>(*region) : nullopt),
        shutdown_(false),
        thread_("SdlThreadVideo", std::bind(&Impl::ThreadMain, this)) {}
  ~Impl() {
    shutdown_.store(true, std::memory_order_relaxed);
    thread_.join();
  }

  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(Impl);

 private:
  void ThreadMain() {
    while (!shutdown_.load(std::memory_order_relaxed)) {
      const double delay =
          renderer_->Render(region_.has_value() ? &region_.value() : nullptr);
      SDL_RenderPresent(renderer_->GetRenderer());

      util::Clock::Instance.SleepSeconds(delay);
    }
  }

  SdlThreadVideoRenderer* renderer_;
  const optional<SDL_Rect> region_;
  std::atomic<bool> shutdown_;
  Thread thread_;
};


SdlManualVideoRenderer::SdlManualVideoRenderer(SDL_Renderer* renderer)
    : impl_(new Impl(renderer)) {}
SdlManualVideoRenderer::~SdlManualVideoRenderer() {}

void SdlManualVideoRenderer::SetRenderer(SDL_Renderer* renderer) {
  impl_->SetRenderer(renderer);
}
SDL_Renderer* SdlManualVideoRenderer::GetRenderer() const {
  return impl_->GetRenderer();
}

double SdlManualVideoRenderer::Render(const SDL_Rect* region) {
  return impl_->Render(region);
}

void SdlManualVideoRenderer::OnSeek() {
  impl_->OnSeek();
}

void SdlManualVideoRenderer::SetPlayer(const MediaPlayer* player) {
  impl_->SetPlayer(player);
}
void SdlManualVideoRenderer::Attach(const DecodedStream* stream) {
  impl_->Attach(stream);
}
void SdlManualVideoRenderer::Detach() {
  impl_->Detach();
}
VideoPlaybackQuality SdlManualVideoRenderer::VideoPlaybackQuality() const {
  return impl_->VideoPlaybackQuality();
}
bool SdlManualVideoRenderer::SetVideoFillMode(VideoFillMode mode) {
  return impl_->SetVideoFillMode(mode);
}


SdlThreadVideoRenderer::SdlThreadVideoRenderer(SDL_Renderer* renderer)
    : SdlManualVideoRenderer(renderer), impl_(new Impl(this, nullptr)) {}
SdlThreadVideoRenderer::SdlThreadVideoRenderer(SDL_Renderer* renderer,
                                               const SDL_Rect* region)
    : SdlManualVideoRenderer(renderer), impl_(new Impl(this, region)) {}
SdlThreadVideoRenderer::~SdlThreadVideoRenderer() {}

}  // namespace media
}  // namespace shaka
