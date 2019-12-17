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

#include "src/media/ios/ios_decoded_frame.h"

#include <utility>

namespace shaka {
namespace media {
namespace ios {

IosDecodedFrame::IosDecodedFrame(std::shared_ptr<const StreamInfo> stream,
                                 double time, double duration,
                                 SampleFormat format, uint32_t sample_count,
                                 std::vector<uint8_t> buffer)
    : DecodedFrame(stream, time, time, duration, format, sample_count,
                   {buffer.data()}, {buffer.size()}),
      data_(std::move(buffer)) {}

IosDecodedFrame::IosDecodedFrame(std::shared_ptr<const StreamInfo> stream,
                                 double time, double duration,
                                 CVImageBufferRef img)
    : DecodedFrame(stream, time, time, duration, PixelFormat::VideoToolbox, 0,
                   {reinterpret_cast<const uint8_t*>(img)}, {0}),
      img_(util::CFRef<CVImageBufferRef>::Acquire(img)) {}

IosDecodedFrame::~IosDecodedFrame() {}

}  // namespace ios
}  // namespace media
}  // namespace shaka
