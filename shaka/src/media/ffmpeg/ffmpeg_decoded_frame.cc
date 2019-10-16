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

#include "src/media/ffmpeg/ffmpeg_decoded_frame.h"

#include <glog/logging.h>

extern "C" {
#include <libavutil/imgutils.h>
}

namespace shaka {
namespace media {
namespace ffmpeg {

namespace {

bool MapFrameFormat(bool is_video, AVFrame* frame,
                    variant<PixelFormat, SampleFormat>* format) {
  if (is_video) {
    switch (frame->format) {
      case AV_PIX_FMT_YUV420P:
        *format = PixelFormat::YUV420P;
        return true;
      case AV_PIX_FMT_NV12:
        *format = PixelFormat::NV12;
        return true;
      case AV_PIX_FMT_RGB24:
        *format = PixelFormat::RGB24;
        return true;

      case AV_PIX_FMT_VIDEOTOOLBOX:
        *format = PixelFormat::VIDEO_TOOLBOX;
        return true;

      default:
        LOG(DFATAL) << "Unknown pixel format: "
                    << av_get_pix_fmt_name(
                           static_cast<AVPixelFormat>(frame->format));
        return false;
    }
  } else {
    switch (frame->format) {
      case AV_SAMPLE_FMT_U8:
        *format = SampleFormat::PackedU8;
        return true;
      case AV_SAMPLE_FMT_S16:
        *format = SampleFormat::PackedS16;
        return true;
      case AV_SAMPLE_FMT_S32:
        *format = SampleFormat::PackedS32;
        return true;
      case AV_SAMPLE_FMT_S64:
        *format = SampleFormat::PackedS64;
        return true;
      case AV_SAMPLE_FMT_FLT:
        *format = SampleFormat::PackedFloat;
        return true;
      case AV_SAMPLE_FMT_DBL:
        *format = SampleFormat::PackedDouble;
        return true;

      case AV_SAMPLE_FMT_U8P:
        *format = SampleFormat::PlanarU8;
        return true;
      case AV_SAMPLE_FMT_S16P:
        *format = SampleFormat::PlanarS16;
        return true;
      case AV_SAMPLE_FMT_S32P:
        *format = SampleFormat::PlanarS32;
        return true;
      case AV_SAMPLE_FMT_S64P:
        *format = SampleFormat::PlanarS64;
        return true;
      case AV_SAMPLE_FMT_FLTP:
        *format = SampleFormat::PlanarFloat;
        return true;
      case AV_SAMPLE_FMT_DBLP:
        *format = SampleFormat::PlanarDouble;
        return true;

      default:
        LOG(DFATAL) << "Unknown sample format: "
                    << av_get_sample_fmt_name(
                           static_cast<AVSampleFormat>(frame->format));
        return false;
    }
  }
}

}  // namespace

FFmpegDecodedFrame::FFmpegDecodedFrame(
    AVFrame* frame, double pts, double dts, double duration,
    variant<PixelFormat, SampleFormat> format,
    const std::vector<const uint8_t*>& data,
    const std::vector<size_t>& linesize)
    : DecodedFrame(pts, dts, duration, format, frame->width, frame->height,
                   frame->channels, frame->sample_rate, frame->nb_samples, data,
                   linesize),
      frame_(frame) {}

FFmpegDecodedFrame::~FFmpegDecodedFrame() {
  av_frame_unref(frame_);
  av_frame_free(&frame_);
}

// static
FFmpegDecodedFrame* FFmpegDecodedFrame::CreateFrame(bool is_video,
                                                    AVFrame* frame, double time,
                                                    double duration) {
  variant<PixelFormat, SampleFormat> format;
  if (!MapFrameFormat(is_video, frame, &format))
    return nullptr;

  std::vector<const uint8_t*> data;
  std::vector<size_t> linesize;
  if (is_video && get<PixelFormat>(format) == PixelFormat::VIDEO_TOOLBOX) {
    data.emplace_back(frame->data[3]);
    linesize.emplace_back(0);
  } else {
    const size_t count = GetPlaneCount(format, frame->channels);
    data.assign(frame->extended_data, frame->extended_data + count);
    if (is_video) {
      DCHECK_LE(count, sizeof(frame->linesize) / sizeof(frame->linesize[0]));
      for (size_t i = 0; i < count; i++) {
        if (frame->linesize[i] < 0) {
          LOG(DFATAL) << "Negative linesize not supported";
          return nullptr;
        }
        linesize.emplace_back(frame->linesize[i]);
      }
    } else {
      // All audio planes must be the same size.
      linesize.insert(linesize.end(), count, frame->linesize[0]);
    }
  }

  // TODO(modmaker): Add an AVFrame pool to reuse objects.
  AVFrame* copy = av_frame_clone(frame);
  if (!copy)
    return nullptr;
  return new (std::nothrow)
      FFmpegDecodedFrame(copy, time, time, duration, format, data, linesize);
}

size_t FFmpegDecodedFrame::EstimateSize() const {
  size_t size = sizeof(*this) + sizeof(*frame_);
  for (int i = AV_NUM_DATA_POINTERS; i; i--) {
    if (frame_->buf[i - 1])
      size += frame_->buf[i - 1]->size;
  }
  for (int i = frame_->nb_extended_buf; i; i--)
    size += frame_->extended_buf[i - 1]->size;
  for (int i = frame_->nb_side_data; i; i--)
    size += frame_->side_data[i - 1]->size;
  return size;
}

}  // namespace ffmpeg
}  // namespace media
}  // namespace shaka
