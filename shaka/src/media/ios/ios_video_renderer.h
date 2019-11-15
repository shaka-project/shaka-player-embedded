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

#ifndef SHAKA_EMBEDDED_MEDIA_IOS_IOS_VIDEO_RENDERER_H_
#define SHAKA_EMBEDDED_MEDIA_IOS_IOS_VIDEO_RENDERER_H_

#include <VideoToolbox/VideoToolbox.h>

#include <memory>

#include "shaka/media/frames.h"
#include "src/media/video_renderer_common.h"

namespace shaka {
namespace media {
namespace ios {

/**
 * Defines a video renderer that renders to an iOS CGImageRef.
 */
class IosVideoRenderer : public VideoRendererCommon {
 public:
  IosVideoRenderer();
  ~IosVideoRenderer() override;

  /**
   * Renders the current video frame to a new image.  This follows the CREATE
   * rule.
   */
  CGImageRef Render();

 private:
  CGImageRef RenderPackedFrame(std::shared_ptr<DecodedFrame> frame);
  CGImageRef RenderPlanarFrame(std::shared_ptr<DecodedFrame> frame);
};

}  // namespace ios
}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_IOS_IOS_VIDEO_RENDERER_H_
