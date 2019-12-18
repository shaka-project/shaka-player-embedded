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

#include "src/test/frame_converter.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace shaka {

namespace {

int GetFFmpegFormat(variant<media::PixelFormat, media::SampleFormat> format) {
  switch (get<media::PixelFormat>(format)) {
    case media::PixelFormat::YUV420P:
      return AV_PIX_FMT_YUV420P;
    case media::PixelFormat::NV12:
      return AV_PIX_FMT_NV12;
    case media::PixelFormat::RGB24:
      return AV_PIX_FMT_RGB24;
    case media::PixelFormat::VideoToolbox:
      return AV_PIX_FMT_VIDEOTOOLBOX;

    default:
      return AV_PIX_FMT_NONE;
  }
}

}  // namespace

FrameConverter::FrameConverter() {}
FrameConverter::~FrameConverter() {
  sws_freeContext(sws_ctx_);
  av_freep(&convert_frame_data_[0]);
}

bool FrameConverter::ConvertFrame(std::shared_ptr<media::DecodedFrame> frame,
                                  const uint8_t** data, size_t* size) {
  if (frame->stream_info->width != convert_frame_width_ ||
      frame->stream_info->height != convert_frame_height_) {
    av_freep(&convert_frame_data_[0]);
    if (av_image_alloc(convert_frame_data_, convert_frame_linesize_,
                       frame->stream_info->width, frame->stream_info->height,
                       AV_PIX_FMT_ARGB, 16) < 0) {
      LOG(ERROR) << "Error allocating frame for conversion";
      return false;
    }
    convert_frame_width_ = frame->stream_info->width;
    convert_frame_height_ = frame->stream_info->height;
  }

  sws_ctx_ = sws_getCachedContext(
      sws_ctx_, frame->stream_info->width, frame->stream_info->height,
      static_cast<AVPixelFormat>(GetFFmpegFormat(frame->format)),
      frame->stream_info->width, frame->stream_info->height, AV_PIX_FMT_ARGB, 0,
      nullptr, nullptr, nullptr);
  if (!sws_ctx_) {
    LOG(ERROR) << "Error allocating conversion context";
    return false;
  }

  std::vector<int> linesize{frame->linesize.begin(), frame->linesize.end()};
  sws_scale(sws_ctx_, frame->data.data(), linesize.data(), 0,
            frame->stream_info->height, convert_frame_data_,
            convert_frame_linesize_);

  *data = convert_frame_data_[0];
  *size = convert_frame_linesize_[0] * frame->stream_info->height;
  return true;
}

}  // namespace shaka
