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

#include "src/media/ffmpeg_decoded_frame.h"

namespace shaka {
namespace media {

FFmpegDecodedFrame::FFmpegDecodedFrame(AVFrame* frame, double pts, double dts,
                                       double duration)
    : BaseFrame(pts, dts, duration, true), frame_(frame) {}

FFmpegDecodedFrame::~FFmpegDecodedFrame() {
  av_frame_unref(frame_);
  av_frame_free(&frame_);
}

// static
FFmpegDecodedFrame* FFmpegDecodedFrame::CreateFrame(AVFrame* frame, double time,
                                                    double duration) {
  // TODO(modmaker): Add an AVFrame pool to reuse objects.
  AVFrame* copy = av_frame_clone(frame);
  if (!copy)
    return nullptr;
  return new (std::nothrow) FFmpegDecodedFrame(copy, time, time, duration);
}


uint8_t** FFmpegDecodedFrame::data() const {
  return frame_->data;
}

int* FFmpegDecodedFrame::linesize() const {
  return frame_->linesize;
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

}  // namespace media
}  // namespace shaka
