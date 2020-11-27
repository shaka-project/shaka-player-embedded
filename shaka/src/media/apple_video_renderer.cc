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

#include "shaka/media/apple_video_renderer.h"

#include <glog/logging.h>

#include "src/media/video_renderer_common.h"

namespace shaka {
namespace media {

namespace {

struct FrameInfo {
  std::shared_ptr<shaka::media::DecodedFrame> frame;
  size_t widths[4];
  size_t heights[4];
};

void FreeFrameBytes(void* info, const void*, size_t) {
  auto* frame = reinterpret_cast<FrameInfo*>(info);
  delete frame;
}

void FreeFramePlanar(void* info, const void*, size_t, size_t, const void**) {
  auto* frame = reinterpret_cast<FrameInfo*>(info);
  delete frame;
}

}  // namespace

class AppleVideoRenderer::Impl final : public VideoRendererCommon {
 public:
  CGImageRef Render(double* delay, Rational<uint32_t>* sample_aspect_ratio);

 private:
  CGImageRef RenderPackedFrame(std::shared_ptr<DecodedFrame> frame);
  CGImageRef RenderPlanarFrame(std::shared_ptr<DecodedFrame> frame);

  std::shared_ptr<DecodedFrame> prev_frame_;
};

CGImageRef AppleVideoRenderer::Impl::Render(
    double* delay, Rational<uint32_t>* sample_aspect_ratio) {
  std::shared_ptr<DecodedFrame> frame;
  const double loc_delay = GetCurrentFrame(&frame);
  if (delay)
    *delay = loc_delay;

  const bool is_paused = player_->PlaybackState() == VideoPlaybackState::Paused;
  if (!frame || (frame == prev_frame_ && !is_paused))
    return nullptr;

  if (sample_aspect_ratio)
    *sample_aspect_ratio = frame->stream_info->sample_aspect_ratio;
  prev_frame_ = frame;
  switch (get<PixelFormat>(frame->format)) {
    case PixelFormat::RGB24:
      return RenderPackedFrame(frame);

    case PixelFormat::VideoToolbox:
    case PixelFormat::YUV420P:
      return RenderPlanarFrame(frame);

    default:
      LOG(DFATAL) << "Unsupported pixel format: " << frame->format;
      return nullptr;
  }
}

CGImageRef AppleVideoRenderer::Impl::RenderPackedFrame(
    std::shared_ptr<DecodedFrame> frame) {
  const uint32_t width = frame->stream_info->width;
  const uint32_t height = frame->stream_info->height;
  const size_t bytes_per_row = frame->linesize[0];
  CFIndex size = bytes_per_row * height;

  // TODO: Handle padding.

  // Make a CGDataProvider object to distribute the data to the CGImage.
  // This takes ownership of the frame and calls the given callback when the
  // CGImage is destroyed.
  const uint8_t* data = frame->data[0];
  auto* info = new FrameInfo;
  info->frame = frame;
  CGDataProviderRef provider =
      CGDataProviderCreateWithData(info, data, size, &FreeFrameBytes);

  // CGColorSpaceCreateDeviceRGB makes a device-specific colorSpace, so use a
  // standardized one instead.
  CGColorSpaceRef color_space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);

  // Create a CGImage.
  const size_t bits_per_pixel = 24;
  const size_t bits_per_component = 8;
  const bool should_interpolate = false;
  CGImage* image = CGImageCreate(width, height, bits_per_component,
                                 bits_per_pixel, bytes_per_row, color_space,
                                 kCGBitmapByteOrderDefault, provider, nullptr,
                                 should_interpolate, kCGRenderingIntentDefault);

  // Dispose of temporary data.
  CGColorSpaceRelease(color_space);
  CGDataProviderRelease(provider);

  return image;
}

CGImageRef AppleVideoRenderer::Impl::RenderPlanarFrame(
    std::shared_ptr<DecodedFrame> frame) {
  CVPixelBufferRef pixel_buffer;
  bool free_pixel_buffer;
  auto pix_fmt = get<PixelFormat>(frame->format);
  if (pix_fmt == shaka::media::PixelFormat::VideoToolbox) {
    uint8_t* data = const_cast<uint8_t*>(frame->data[0]);
    pixel_buffer = reinterpret_cast<CVPixelBufferRef>(data);
    free_pixel_buffer = false;
  } else {
    if (pix_fmt != PixelFormat::YUV420P)
      return nullptr;

    const OSType cv_pix_fmt = kCVPixelFormatType_420YpCbCr8Planar;
    auto* info = new FrameInfo;
    info->frame = frame;
    info->widths[0] = frame->stream_info->width;
    info->widths[1] = info->widths[2] = frame->stream_info->width / 2;
    info->heights[0] = frame->stream_info->height;
    info->heights[1] = info->heights[2] = frame->stream_info->height / 2;

    const auto status = CVPixelBufferCreateWithPlanarBytes(
        nullptr, frame->stream_info->width, frame->stream_info->height,
        cv_pix_fmt, nullptr, 0, frame->data.size(),
        reinterpret_cast<void**>(const_cast<uint8_t**>(frame->data.data())),
        info->widths, info->heights,
        const_cast<size_t*>(frame->linesize.data()), &FreeFramePlanar, info,
        nullptr, &pixel_buffer);
    if (status != 0) {
      LOG(ERROR) << "CVPixelBufferCreateWithPlanarBytes error " << status;
      delete info;
      return nullptr;
    }

    free_pixel_buffer = true;
  }

  CGImage* ret;
  // This retains the buffer, so the Frame is free to be deleted.
  const auto status =
      VTCreateCGImageFromCVPixelBuffer(pixel_buffer, nullptr, &ret);
  if (free_pixel_buffer)
    CVPixelBufferRelease(pixel_buffer);

  if (status != 0) {
    LOG(ERROR) << "VTCreateCGImageFromCVPixelBuffer error " << status;
    return nullptr;
  }
  return ret;
}


AppleVideoRenderer::AppleVideoRenderer() : impl_(new Impl) {}
AppleVideoRenderer::~AppleVideoRenderer() {}

VideoFillMode AppleVideoRenderer::fill_mode() const {
  return impl_->fill_mode();
}

CGImageRef AppleVideoRenderer::Render(double* delay,
                                      Rational<uint32_t>* sample_aspect_ratio) {
  return impl_->Render(delay, sample_aspect_ratio);
}

void AppleVideoRenderer::SetPlayer(const MediaPlayer* player) {
  impl_->SetPlayer(player);
}

void AppleVideoRenderer::Attach(const DecodedStream* stream) {
  impl_->Attach(stream);
}

void AppleVideoRenderer::Detach() {
  impl_->Detach();
}

VideoPlaybackQuality AppleVideoRenderer::VideoPlaybackQuality() const {
  return impl_->VideoPlaybackQuality();
}

bool AppleVideoRenderer::SetVideoFillMode(VideoFillMode mode) {
  return impl_->SetVideoFillMode(mode);
}

}  // namespace media
}  // namespace shaka
