// Copyright 2018 Google LLC
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

#include "shaka/sdl_frame_drawer.h"

#include <list>
#include <unordered_set>

#include "src/util/macros.h"

namespace shaka {

namespace {

constexpr const size_t kMaxTextures = 8;

struct TextureInfo {
  SDL_Texture* texture;
  uint32_t pixel_format;
  int width;
  int height;
};


Uint32 SdlPixelFormatFromPublic(PixelFormat format) {
  switch (format) {
#if SDL_VERSION_ATLEAST(2, 0, 4)
    case PixelFormat::NV12:
      return SDL_PIXELFORMAT_NV12;
#endif
    case PixelFormat::YUV420P:
      return SDL_PIXELFORMAT_IYUV;
    case PixelFormat::RGB24:
      return SDL_PIXELFORMAT_RGB24;

    default:
      return SDL_PIXELFORMAT_UNKNOWN;
  }
}

PixelFormat PublicPixelFormatFromSdl(Uint32 format) {
  switch (format) {
#if SDL_VERSION_ATLEAST(2, 0, 4)
    case SDL_PIXELFORMAT_NV12:
      return PixelFormat::NV12;
#endif
    case SDL_PIXELFORMAT_IYUV:
      return PixelFormat::YUV420P;
    case SDL_PIXELFORMAT_RGB24:
      return PixelFormat::RGB24;

    default:
      return PixelFormat::Unknown;
  }
}

}  // namespace

class SdlFrameDrawer::Impl {
 public:
  Impl() : renderer_(nullptr) {}
  ~Impl() {
    Clear();
  }

  NON_COPYABLE_OR_MOVABLE_TYPE(Impl);

  void SetRenderer(SDL_Renderer* renderer) {
    Clear();
    texture_formats_.clear();
    renderer_ = renderer;

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) == 0) {
      texture_formats_.insert(info.texture_formats,
                              info.texture_formats + info.num_texture_formats);
    }
    if (texture_formats_.empty()) {
      LOG(ERROR) << "No supported texture formats";
    }
  }

  SDL_Texture* Draw(Frame* frame) {
    if (!frame || !frame->valid())
      return nullptr;

    auto sdl_pix_fmt = SdlPixelFormatFromPublic(frame->pixel_format());
    if (sdl_pix_fmt == SDL_PIXELFORMAT_UNKNOWN ||
        texture_formats_.count(sdl_pix_fmt) == 0) {
      if (!Convert(frame))
        return nullptr;
      sdl_pix_fmt = SdlPixelFormatFromPublic(frame->pixel_format());
    }

    SDL_Texture* texture =
        GetTexture(sdl_pix_fmt, frame->width(), frame->height());
    if (!texture)
      return nullptr;

    if (!DrawOntoTexture(frame, texture, sdl_pix_fmt))
      return nullptr;

    return texture;
  }

 private:
  bool DrawOntoTexture(Frame* frame, SDL_Texture* texture, Uint32 sdl_pix_fmt) {
    const uint8_t* const* frame_data = frame->data();
    const int* frame_linesize = frame->linesize();

    // TODO: It is possible for linesize values to be negative.  Handle this
    // case: https://www.ffmpeg.org/doxygen/trunk/ffplay_8c_source.html#l00905
    DCHECK_GE(frame_linesize[0], 0);
    if (sdl_pix_fmt == SDL_PIXELFORMAT_IYUV) {
      if (SDL_UpdateYUVTexture(
              texture, nullptr, frame_data[0], frame_linesize[0], frame_data[1],
              frame_linesize[1], frame_data[2], frame_linesize[2]) < 0) {
        LOG(ERROR) << "Error updating texture: " << SDL_GetError();
        return false;
      }
#if SDL_VERSION_ATLEAST(2, 0, 4)
    } else if (sdl_pix_fmt == SDL_PIXELFORMAT_NV12 ||
               sdl_pix_fmt == SDL_PIXELFORMAT_NV21) {
      uint8_t* pixels;
      int pitch;
      if (SDL_LockTexture(texture, nullptr, reinterpret_cast<void**>(&pixels),
                          &pitch) < 0) {
        LOG(ERROR) << "Error locking texture: " << SDL_GetError();
        return false;
      }

      if (pitch == frame_linesize[0]) {
        // TODO: Sometimes there is a green bar at the bottom.
        const size_t size0 = pitch * frame->height();
        memcpy(pixels, frame_data[0], size0);
        memcpy(pixels + size0, frame_data[1], size0 / 2);
      } else {
        // FFmpeg may add padding to the rows, so we need to drop it by manually
        // copying each line.
        DCHECK_GE(frame_linesize[0], pitch);
        const int min_width = std::min(pitch, frame_linesize[0]);
        for (uint32_t row = 0; row < frame->height(); row++) {
          uint8_t* dest = pixels + pitch * row;
          const uint8_t* src = frame_data[0] + frame_linesize[0] * row;
          memcpy(dest, src, min_width);
        }
        for (uint32_t row = 0; row < frame->height() / 2; row++) {
          uint8_t* dest = pixels + pitch * (row + frame->height());
          const uint8_t* src = frame_data[1] + frame_linesize[1] * row;
          memcpy(dest, src, min_width);
        }
      }

      SDL_UnlockTexture(texture);
#endif
    } else {
      if (SDL_UpdateTexture(texture, nullptr, frame_data[0],
                            frame_linesize[0]) < 0) {
        LOG(ERROR) << "Error updating texture: " << SDL_GetError();
        return false;
      }
    }

    return true;
  }

  bool Convert(Frame* frame) {
    for (Uint32 sdl_fmt : texture_formats_) {
      auto public_fmt = PublicPixelFormatFromSdl(sdl_fmt);
      if (public_fmt != PixelFormat::Unknown && frame->ConvertTo(public_fmt))
        return true;
    }
    return false;
  }

  SDL_Texture* GetTexture(Uint32 pixel_format, int width, int height) {
    if (!renderer_)
      return nullptr;

    for (auto it = textures_.begin(); it != textures_.end(); it++) {
      if (it->pixel_format == pixel_format && it->width == width &&
          it->height == height) {
        if (std::next(it) != textures_.end()) {
          // Move the texture to the end so elements at the beginning are ones
          // that were least-recently used.
          textures_.splice(textures_.end(), textures_, it);
        }

        return it->texture;
      }
    }

    if (!textures_.empty() && textures_.size() >= kMaxTextures) {
      SDL_DestroyTexture(textures_.front().texture);
      textures_.erase(textures_.begin());
      DCHECK_LT(textures_.size(), kMaxTextures);
    }

    SDL_Texture* texture = SDL_CreateTexture(
        renderer_, pixel_format, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (texture)
      textures_.push_back({texture, pixel_format, width, height});
    else
      LOG(ERROR) << "Error creating texture: " << SDL_GetError();

    return texture;
  }

  void Clear() {
    for (auto& info : textures_)
      SDL_DestroyTexture(info.texture);
  }

  std::list<TextureInfo> textures_;
  std::unordered_set<Uint32> texture_formats_;
  SDL_Renderer* renderer_;
};

SdlFrameDrawer::SdlFrameDrawer() : impl_(new Impl) {}
SdlFrameDrawer::SdlFrameDrawer(SdlFrameDrawer&&) = default;
SdlFrameDrawer::~SdlFrameDrawer() {}
SdlFrameDrawer& SdlFrameDrawer::operator=(SdlFrameDrawer&&) = default;

void SdlFrameDrawer::SetRenderer(SDL_Renderer* renderer) {
  impl_->SetRenderer(renderer);
}

SDL_Texture* SdlFrameDrawer::Draw(Frame* frame) {
  return impl_->Draw(frame);
}

}  // namespace shaka
