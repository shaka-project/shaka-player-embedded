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

#include "src/media/ios/ios_video_renderer.h"

#include <glog/logging.h>

namespace shaka {
namespace media {
namespace ios {

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

IosVideoRenderer::IosVideoRenderer() {}
IosVideoRenderer::~IosVideoRenderer() {}

CGImageRef IosVideoRenderer::Render() {
  std::shared_ptr<DecodedFrame> frame;
  GetCurrentFrame(&frame);

  if (!frame)
    return nullptr;

  switch (get<PixelFormat>(frame->format)) {
    case PixelFormat::RGB24:
      return RenderPackedFrame(frame);

    case PixelFormat::VideoToolbox:
    case PixelFormat::YUV420P:
      return RenderPlanarFrame(frame);

    default:
      LOG(DFATAL) << "Unsupported pixel format: "
                  << static_cast<uint8_t>(get<PixelFormat>(frame->format));
      return nullptr;
  }
}

CGImageRef IosVideoRenderer::RenderPackedFrame(
    std::shared_ptr<DecodedFrame> frame) {
  const uint32_t width = frame->width;
  const uint32_t height = frame->height;
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

CGImageRef IosVideoRenderer::RenderPlanarFrame(
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

    const OSType ios_pix_fmt = kCVPixelFormatType_420YpCbCr8Planar;
    auto* info = new FrameInfo;
    info->frame = frame;
    info->widths[0] = frame->width;
    info->widths[1] = info->widths[2] = frame->width / 2;
    info->heights[0] = frame->height;
    info->heights[1] = info->heights[2] = frame->height / 2;

    const auto status = CVPixelBufferCreateWithPlanarBytes(
        nullptr, frame->width, frame->height, ios_pix_fmt, nullptr, 0,
        frame->data.size(),
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

}  // namespace ios
}  // namespace media
}  // namespace shaka
