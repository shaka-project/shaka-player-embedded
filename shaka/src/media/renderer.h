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

#ifndef SHAKA_EMBEDDED_MEDIA_RENDERER_H_
#define SHAKA_EMBEDDED_MEDIA_RENDERER_H_

#include <memory>

#include "shaka/media/frames.h"

namespace shaka {
namespace media {

/**
 * Defines a base class for a renderer.  Derived classes will handle rendering
 * the decompressed frames.  For example, video streams will draw frames to a
 * texture so the app can draw it to the screen.
 */
class Renderer {
 public:
  virtual ~Renderer() {}

  /**
   * For a video renderer, this will draw a frame to a texture and return it.
   * For other renderers, this returns an invalid frame.
   *
   * @param dropped_frame_count [OUT] Will contain the number of frames that
   *   were dropped since the last call to DrawFrame.
   * @param is_new_frame [OUT] Will contain whether this frame is a different
   *   frame than the previous call to DrawFrame.
   * @param delay [OUT] Optional, will contain the delay, in seconds, until the
   *   next frame should be drawn.
   */
  virtual std::shared_ptr<DecodedFrame> DrawFrame(int* dropped_frame_count,
                                                  bool* is_new_frame,
                                                  double* delay);

  /** Called when the video seeks. */
  virtual void OnSeek();

  /**
   * Called when a seek has been completed.  This happens once the first frame
   * at the new time has been decoded.
   */
  virtual void OnSeekDone();
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_RENDERER_H_
