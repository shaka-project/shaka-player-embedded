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

#ifndef SHAKA_EMBEDDED_MEDIA_MEDIA_VIDEO_RENDERER_H_
#define SHAKA_EMBEDDED_MEDIA_MEDIA_VIDEO_RENDERER_H_

#include <functional>
#include <memory>

#include "src/debug/mutex.h"
#include "src/media/renderer.h"
#include "src/util/macros.h"

namespace shaka {
namespace media {

class Stream;

/**
 * Defines a renderer that draws video frames to the screen.
 */
class VideoRenderer : public Renderer {
 public:
  VideoRenderer(std::function<double()> get_time, Stream* stream);
  ~VideoRenderer() override;

  NON_COPYABLE_OR_MOVABLE_TYPE(VideoRenderer);

  std::shared_ptr<DecodedFrame> DrawFrame(int* dropped_frame_count,
                                          bool* is_new_frame,
                                          double* delay) override;
  void OnSeek() override;
  void OnSeekDone() override;

 private:
  Mutex mutex_;
  Stream* const stream_;
  const std::function<double()> get_time_;
  double prev_time_;
  bool is_seeking_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_MEDIA_VIDEO_RENDERER_H_
