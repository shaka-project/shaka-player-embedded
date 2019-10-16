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

#include "src/media/frame_converter.h"

extern "C" {
#include <libavutil/hwcontext.h>
}

namespace shaka {
namespace media {

namespace {

bool IsHardwarePixelFormat(AVPixelFormat format) {
  switch (format) {
    case AV_PIX_FMT_VIDEOTOOLBOX:
    case AV_PIX_FMT_VAAPI:
    case AV_PIX_FMT_VDPAU:
    case AV_PIX_FMT_QSV:
    case AV_PIX_FMT_MMAL:
    case AV_PIX_FMT_D3D11VA_VLD:
    case AV_PIX_FMT_CUDA:
    case AV_PIX_FMT_XVMC:
    case AV_PIX_FMT_MEDIACODEC:
    case AV_PIX_FMT_D3D11:
    case AV_PIX_FMT_OPENCL:
      return true;

    default:
      return false;
  }
}

}  // namespace

FrameConverter::FrameConverter() : cpu_frame_(nullptr) {}
FrameConverter::~FrameConverter() {
  if (cpu_frame_)
    av_frame_free(&cpu_frame_);

#ifdef HAS_SWSCALE
  sws_freeContext(sws_ctx_);
  av_freep(&convert_frame_data_[0]);
#endif
}

bool FrameConverter::ConvertFrame(const AVFrame* frame,
                                  const uint8_t* const** data,
                                  const int** linesize,
                                  AVPixelFormat desired_pixel_format) {
  if (IsHardwarePixelFormat(static_cast<AVPixelFormat>(frame->format))) {
    if (!cpu_frame_) {
      cpu_frame_ = av_frame_alloc();
      if (!cpu_frame_) {
        LOG(ERROR) << "Error allocating frame for conversion";
        return false;
      }
    }

    av_frame_unref(cpu_frame_);
    const int copy_code = av_hwframe_transfer_data(cpu_frame_, frame, 0);
    if (copy_code < 0) {
      LOG(ERROR) << "Error transferring frame data to CPU: " << copy_code;
      return false;
    }
    frame = cpu_frame_;
  }

  if (frame->format == desired_pixel_format) {
    *data = frame->data;
    *linesize = frame->linesize;
    return true;
  }

#ifdef HAS_SWSCALE
  if (frame->width != convert_frame_width_ ||
      frame->height != convert_frame_height_ ||
      desired_pixel_format != convert_pixel_format_) {
    av_freep(&convert_frame_data_[0]);
    if (av_image_alloc(convert_frame_data_, convert_frame_linesize_,
                       frame->width, frame->height, desired_pixel_format,
                       16) < 0) {
      LOG(ERROR) << "Error allocating frame for conversion";
      return false;
    }
    convert_frame_width_ = frame->width;
    convert_frame_height_ = frame->height;
    convert_pixel_format_ = desired_pixel_format;
  }

  sws_ctx_ = sws_getCachedContext(
      sws_ctx_, frame->width, frame->height,
      static_cast<AVPixelFormat>(frame->format), frame->width, frame->height,
      desired_pixel_format, 0, nullptr, nullptr, nullptr);
  if (!sws_ctx_) {
    LOG(ERROR) << "Error allocating conversion context";
    return false;
  }

  sws_scale(sws_ctx_, frame->data, frame->linesize, 0, frame->height,
            convert_frame_data_, convert_frame_linesize_);

  *data = convert_frame_data_;
  *linesize = convert_frame_linesize_;
  return true;
#else
  LOG_ONCE(INFO) << "Not built to convert pixel formats";
  return false;
#endif
}

}  // namespace media
}  // namespace shaka
