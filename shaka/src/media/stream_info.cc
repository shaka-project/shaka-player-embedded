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

#include "shaka/media/stream_info.h"

#include <unordered_map>

namespace shaka {
namespace media {

class StreamInfo::Impl {};

StreamInfo::StreamInfo(const std::string& mime, const std::string& codec,
                       bool is_video, Rational<uint32_t> time_scale,
                       Rational<uint32_t> sample_aspect_ratio,
                       const std::vector<uint8_t>& extra_data, uint32_t width,
                       uint32_t height, uint32_t channel_count,
                       uint32_t sample_rate)
    : mime_type(mime),
      codec(codec),
      time_scale(time_scale),
      sample_aspect_ratio(sample_aspect_ratio),
      extra_data(extra_data),
      is_video(is_video),
      width(width),
      height(height),
      channel_count(channel_count),
      sample_rate(sample_rate) {}
StreamInfo::~StreamInfo() {}

}  // namespace media
}  // namespace shaka
