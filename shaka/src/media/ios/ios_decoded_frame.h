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

#ifndef SHAKA_EMBEDDED_MEDIA_IOS_IOS_DECODED_FRAME_H_
#define SHAKA_EMBEDDED_MEDIA_IOS_IOS_DECODED_FRAME_H_

#include <CoreVideo/CoreVideo.h>

#include <memory>
#include <vector>

#include "shaka/media/frames.h"
#include "src/util/cfref.h"

namespace shaka {
namespace media {
namespace ios {

/** This defines a single decoded media frame. */
class IosDecodedFrame final : public DecodedFrame {
 public:
  // An audio frame.
  IosDecodedFrame(std::shared_ptr<const StreamInfo> stream, double time,
                  double duration, SampleFormat format, uint32_t sample_count,
                  std::vector<uint8_t> buffer);
  // A video frame.
  IosDecodedFrame(std::shared_ptr<const StreamInfo> stream, double time,
                  double duration, CVImageBufferRef img);
  ~IosDecodedFrame() override;

 private:
  util::CFRef<CVImageBufferRef> img_;
  std::vector<uint8_t> data_;
};

}  // namespace ios
}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_IOS_IOS_DECODED_FRAME_H_
