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

#ifndef SHAKA_EMBEDDED_SDL_FRAME_DRAWER_H_
#define SHAKA_EMBEDDED_SDL_FRAME_DRAWER_H_

#include <SDL2/SDL.h>

#include <memory>

#include "media/frames.h"
#include "macros.h"

namespace shaka {

/**
 * A helper class that is used to convert Shaka Embedded Frame objects into an
 * SDL texture.
 *
 * @ingroup utils
 */
class SHAKA_EXPORT SdlFrameDrawer final {
 public:
  SdlFrameDrawer();
  SdlFrameDrawer(const SdlFrameDrawer&) = delete;
  SdlFrameDrawer(SdlFrameDrawer&&);
  ~SdlFrameDrawer();

  SdlFrameDrawer& operator=(const SdlFrameDrawer&) = delete;
  SdlFrameDrawer& operator=(SdlFrameDrawer&&);


  /**
   * Sets the renderer used to create textures.  This MUST be called at least
   * once before calling Draw.  This can be changed at any time, but will
   * invalidate any existing textures.
   */
  void SetRenderer(SDL_Renderer* renderer);

  /**
   * Draws the given frame onto a texture.  This may invalidate any existing
   * textures.
   *
   * @param frame The frame to draw.
   * @return The created texture, or nullptr on error.
   */
  SDL_Texture* Draw(std::shared_ptr<media::DecodedFrame> frame);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_SDL_FRAME_DRAWER_H_
