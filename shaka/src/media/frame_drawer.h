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

#ifndef SHAKA_EMBEDDED_MEDIA_MEDIA_FRAME_DRAWER_H_
#define SHAKA_EMBEDDED_MEDIA_MEDIA_FRAME_DRAWER_H_

#include <memory>

#include "shaka/frame.h"
#include "src/util/macros.h"

namespace shaka {
namespace media {

class BaseFrame;

/**
 * Defines an interface to drawing frames.  This takes a decoded frame and
 * outputs a hardware texture.  This is an abstraction between SDL2 textures
 * and native drawing.  This also handles converting software frames to a
 * texture as needed.
 */
class FrameDrawer {
 public:
  FrameDrawer();
  virtual ~FrameDrawer();

  NON_COPYABLE_OR_MOVABLE_TYPE(FrameDrawer);

  virtual Frame DrawFrame(const BaseFrame* frame);
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_MEDIA_FRAME_DRAWER_H_
