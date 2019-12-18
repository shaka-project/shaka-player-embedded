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

#ifndef SHAKA_EMBEDDED_MEDIA_SDL_VIDEO_RENDERER_H_
#define SHAKA_EMBEDDED_MEDIA_SDL_VIDEO_RENDERER_H_

#include <memory>
#include <string>

#include <SDL2/SDL.h>

#include "../macros.h"
#include "renderer.h"

namespace shaka {
namespace media {

/**
 * Defines a video renderer that renders frames to the given SDL window when
 * specified by the app.  The app is expected to periodically call Render to
 * render the frame.  This allows the app to draw the frames as part of their
 * normal render loop.
 *
 * @ingroup media
 */
class SHAKA_EXPORT SdlManualVideoRenderer : public VideoRenderer {
 public:
  /**
   * Creates a new renderer that renders using the given renderer object.
   *
   * @param renderer The renderer object used to create textures.  If not given,
   *   apps must call SetRenderer before calling Render.
   */
  SdlManualVideoRenderer(SDL_Renderer* renderer = nullptr);
  ~SdlManualVideoRenderer() override;

  /**
   * Sets the renderer used to create textures.  Changing the renderer during
   * playback causes performance loss since internally-cached textures are
   * invalidated.  It is assumed this renderer will live as long as this object
   * or to another call to SetRenderer.
   *
   * @param renderer The renderer used to create textures.
   */
  void SetRenderer(SDL_Renderer* renderer);

  /** @return The current renderer used to draw frames. */
  SDL_Renderer* GetRenderer() const;

  /**
   * Renders the current video frame to the given sub-region of the current
   * renderer.
   *
   * @param region The region to draw the video to.  If not given, video will
   *   take up the entire window.
   * @return The suggested delay, in seconds, before the next call to Render;
   *   this uses the framerate and current time to suggest the next delay.
   */
  double Render(SDL_Rect* region = nullptr);


  void OnSeek() override;

  void SetPlayer(const MediaPlayer* player) override;
  void Attach(const DecodedStream* stream) override;
  void Detach() override;

  struct VideoPlaybackQuality VideoPlaybackQuality() const override;
  bool SetVideoFillMode(VideoFillMode mode) override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * Defines a video renderer that renders frames to the given SDL window on a
 * background thread.  This periodically draws the frame onto the renderer and
 * presents it.  This is best used for full-screen apps since this can cause
 * synchronization issues if other threads try to draw to the same renderer.
 * Alternatively, use the SdlVideoManualRenderer.
 *
 * @ingroup media
 */
class SHAKA_EXPORT SdlThreadVideoRenderer final
    : public SdlManualVideoRenderer {
 public:
  /**
   * Creates a new renderer that draws video frames using the given renderer.
   *
   * @param renderer The renderer used to draw frames.
   */
  SdlThreadVideoRenderer(SDL_Renderer* renderer);
  /**
   * Creates a new renderer that draws video frames using the given renderer.
   * This renders to the given sub-region of the window.
   *
   * @param renderer The renderer used to draw frames.
   * @param region The sub-region to draw to.
   */
  SdlThreadVideoRenderer(SDL_Renderer* renderer, SDL_Rect region);
  ~SdlThreadVideoRenderer() override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_SDL_VIDEO_RENDERER_H_
