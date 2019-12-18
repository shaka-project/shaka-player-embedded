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

#include "shaka/media/frames.h"

#include <glog/logging.h>

namespace shaka {
namespace media {

bool IsPlanarFormat(variant<PixelFormat, SampleFormat> format) {
  if (holds_alternative<PixelFormat>(format)) {
    switch (get<PixelFormat>(format)) {
      case PixelFormat::YUV420P:
      case PixelFormat::NV12:
        return true;
      default:
        return false;
    }
  } else {
    switch (get<SampleFormat>(format)) {
      case SampleFormat::PlanarU8:
      case SampleFormat::PlanarS16:
      case SampleFormat::PlanarS32:
      case SampleFormat::PlanarS64:
      case SampleFormat::PlanarFloat:
      case SampleFormat::PlanarDouble:
        return true;
      default:
        return false;
    }
  }
}

size_t GetPlaneCount(variant<PixelFormat, SampleFormat> format,
                     size_t channels) {
  if (holds_alternative<PixelFormat>(format)) {
    switch (get<PixelFormat>(format)) {
      case PixelFormat::YUV420P:
        return 3;
      case PixelFormat::NV12:
        return 2;
      case PixelFormat::RGB24:
      case PixelFormat::VideoToolbox:
        return 1;

      default:
        return 0;
    }
  } else {
    return IsPlanarFormat(format) ? channels : 1;
  }
}


BaseFrame::BaseFrame(std::shared_ptr<const StreamInfo> stream_info, double pts,
                     double dts, double duration, bool is_key_frame)
    : stream_info(stream_info),
      pts(pts),
      dts(dts),
      duration(duration),
      is_key_frame(is_key_frame) {}
BaseFrame::~BaseFrame() {}

size_t BaseFrame::EstimateSize() const {
  return sizeof(*this);
}


EncodedFrame::EncodedFrame(std::shared_ptr<const StreamInfo> stream, double pts,
                           double dts, double duration, bool is_key_frame,
                           const uint8_t* data, size_t data_size,
                           double timestamp_offset, bool is_encrypted)
    : BaseFrame(stream, pts, dts, duration, is_key_frame),
      data(data),
      data_size(data_size),
      timestamp_offset(timestamp_offset),
      is_encrypted(is_encrypted) {}
EncodedFrame::~EncodedFrame() {}

MediaStatus EncodedFrame::Decrypt(const eme::Implementation* implementation,
                                  uint8_t* data) const {
  if (!is_encrypted)
    return MediaStatus::Success;
  LOG(ERROR) << "Decrypt not supported with base EncodedFrame type";
  return MediaStatus::FatalError;
}

size_t EncodedFrame::EstimateSize() const {
  return sizeof(*this) + data_size;
}


DecodedFrame::DecodedFrame(std::shared_ptr<const StreamInfo> stream, double pts,
                           double dts, double duration,
                           variant<PixelFormat, SampleFormat> format,
                           size_t sample_count,
                           const std::vector<const uint8_t*>& data,
                           const std::vector<size_t>& linesize)
    : BaseFrame(stream, pts, dts, duration, true),
      sample_count(sample_count),
      data(data),
      linesize(linesize),
      format(format) {}
DecodedFrame::~DecodedFrame() {}

size_t DecodedFrame::EstimateSize() const {
  size_t ret = sizeof(*this);
  for (size_t line : linesize) {
    if (holds_alternative<PixelFormat>(format))
      ret += line * stream_info->height;
    else
      ret += line;
  }
  return ret;
}

}  // namespace media
}  // namespace shaka
