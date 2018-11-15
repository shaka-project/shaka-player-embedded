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

#include "shaka/frame.h"

#include <glog/logging.h>

#include <type_traits>

#include "src/media/frame_converter.h"
#include "src/util/macros.h"

namespace shaka {

class Frame::Impl {
 public:
  explicit Impl(AVFrame* in_frame) : frame(av_frame_alloc()) {
    CHECK(frame);
    CHECK_EQ(av_frame_ref(frame, in_frame), 0);
    frame_data = frame->data;
    frame_linesize = frame->linesize;

    switch (frame->format) {
      case AV_PIX_FMT_YUV420P:
        format = PixelFormat::YUV420P;
        break;
      case AV_PIX_FMT_NV12:
        format = PixelFormat::NV12;
        break;
      case AV_PIX_FMT_RGB24:
        format = PixelFormat::RGB24;
        break;

      case AV_PIX_FMT_VIDEOTOOLBOX:
        format = PixelFormat::VIDEO_TOOLBOX;
        break;

      default:
        LOG(FATAL) << "Unknown pixel format: "
                   << av_get_pix_fmt_name(
                          static_cast<AVPixelFormat>(frame->format));
    }
  }
  ~Impl() {
    av_frame_free(&frame);
  }

  NON_COPYABLE_OR_MOVABLE_TYPE(Impl);

  media::FrameConverter converter;
  AVFrame* frame;
  uint8_t* const* frame_data;
  const int* frame_linesize;
  PixelFormat format;
};


Frame::Frame() {}
Frame::Frame(AVFrame* frame) : impl_(new Impl(frame)) {}
Frame::Frame(Frame&&) = default;
Frame::~Frame() {}

Frame& Frame::operator=(Frame&&) = default;

bool Frame::valid() const {
  return impl_.get();
}

PixelFormat Frame::pixel_format() const {
  return impl_ ? impl_->format : PixelFormat::Unknown;
}

uint32_t Frame::width() const {
  return impl_ ? impl_->frame->width : 0;
}

uint32_t Frame::height() const {
  return impl_ ? impl_->frame->height : 0;
}

const uint8_t* const* Frame::data() const {
  return impl_ ? impl_->frame_data : nullptr;
}

const int* Frame::linesize() const {
  return impl_ ? impl_->frame_linesize : nullptr;
}

bool Frame::ConvertTo(PixelFormat format) {
  if (!impl_)
    return false;
  if (impl_->format == format)
    return true;

  AVPixelFormat pix_fmt;
  switch (format) {
    case PixelFormat::YUV420P:
      pix_fmt = AV_PIX_FMT_YUV420P;
      break;
    case PixelFormat::NV12:
      pix_fmt = AV_PIX_FMT_NV12;
      break;
    case PixelFormat::RGB24:
      pix_fmt = AV_PIX_FMT_RGB24;
      break;

    case PixelFormat::VIDEO_TOOLBOX:
      LOG(ERROR) << "Cannot convert to a hardware-accelerated format.";
      return false;

    default:
      LOG(FATAL) << "Unknown pixel format: "
                 << static_cast<std::underlying_type<PixelFormat>::type>(
                        format);
  }

  if (!impl_->converter.ConvertFrame(impl_->frame, &impl_->frame_data,
                                     &impl_->frame_linesize, pix_fmt)) {
    return false;
  }

  impl_->format = format;
  return true;
}

}  // namespace shaka
