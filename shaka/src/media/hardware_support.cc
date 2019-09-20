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

#include "src/media/hardware_support.h"

#include <glog/logging.h>

#include <mutex>
#include <unordered_map>

#include "src/util/macros.h"

#if defined(OS_IOS)
#  include <VideoToolbox/VideoToolbox.h>

#  include "src/util/cfref.h"

#  ifndef kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder
// If this were split onto two lines, the \ at the end would be at 81
// characters, and would fail the linter.  So put this on one line and disable
// linting.  This also needs to disable clang-format from fixing it.
// clang-format off
#    define kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder CFSTR("RequireHardwareAcceleratedVideoDecoder")  // NOLINT
// clang-format on
#  endif
#endif

namespace shaka {
namespace media {

namespace {

/** A helper class that memoizes values for the support check. */
class SupportCache {
 public:
  bool TryGet(const std::string& codec, int width, int height, bool* result) {
    std::unique_lock<std::mutex> lock(mutex_);
    CheckArgs args(codec, width, height);
    if (results_.count(args) > 0) {
      *result = results_.at(args);
      return true;
    } else {
      return false;
    }
  }

  void Insert(const std::string& codec, int width, int height, bool result) {
    std::unique_lock<std::mutex> lock(mutex_);
    results_[CheckArgs(codec, width, height)] = result;
  }

 private:
  struct CheckArgs {
    CheckArgs(const std::string& codec, int width, int height)
        : codec(codec), width(width), height(height) {}

    const std::string codec;
    const int width;
    const int height;

    bool operator==(const CheckArgs& other) const noexcept {
      return codec == other.codec && width == other.width &&
             height == other.height;
    }
  };
  struct CheckArgsHash {
    size_t operator()(const CheckArgs& args) const noexcept {
      return std::hash<std::string>()(args.codec) ^ (args.width << 16) ^
             args.height;
    }
  };

  std::mutex mutex_;
  std::unordered_map<CheckArgs, bool, /* Hash */ CheckArgsHash> results_;
};

#ifdef OS_IOS

util::CFRef<CFMutableDictionaryRef> GetExtraData(CMVideoCodecType codec,
                                                 int profile) {
  util::CFRef<CFMutableDictionaryRef> avc_info = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);

  switch (codec) {
    case kCMVideoCodecType_H264: {
      // This is just a common SPS and PPS that doesn't use any "unusual"
      // features; this is believed to be commonly supported.  We can't just
      // pass 0 SPS or PPS, the decoder requires at least one of each.
      uint8_t extra_data[] = {
          // Disable formatting so clang-format doesn't put each element on its
          // own line.
          // clang-format off
          0x01,  // version
          (profile >> 16) & 0xff,  // profile
          (profile >>  8) & 0xff,  // profile compat
          (profile >>  0) & 0xff,  // level
          0xff,  // 6 reserved bits + 2 bits nalu size length - 1

          0xe1,  // 3 reserved bits + 5 bits SPS count
          0x00, 0x1c,  // SPS size
          0x67, 0x42, 0xc8, 0x1e, 0xd9, 0x01, 0x03, 0xfe, 0xbf, 0xf0,
          0x06, 0xe0, 0x06, 0xd1, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00,
          0x00, 0x03, 0x00, 0x30, 0x0f, 0x16, 0x2e, 0x48,

          0x01,  // PPS count
          0x00, 0x04,  // PPS size
          0x68, 0xcb, 0x8c, 0xb2,
          // clang-format on
      };
      util::CFRef<CFDataRef> buffer =
          CFDataCreate(kCFAllocatorDefault, extra_data, sizeof(extra_data));
      if (buffer)
        CFDictionarySetValue(avc_info, CFSTR("avcC"), buffer);
      break;
    }
    default:
      LOG(FATAL) << "Unknown codec type: " << codec;
  }

  return avc_info;
}

void IosDecoderCallback(void*, void*, OSStatus, VTDecodeInfoFlags,
                        CVImageBufferRef, CMTime, CMTime) {}

bool IosHardwareSupport(CMVideoCodecType codec, int profile, int width,
                        int height) {
  if (width * height > 5000000) {
    // VideoToolbox doesn't handle out of memory correctly and has a tendency to
    // just crash with a memory error if we run out of memory.  This only seems
    // to happen with 4k, so just blacklist it for now.
    // TODO: Find a better solution or file a bug.
    return false;
  }

  auto NewDict = []() {
    return CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                     &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
  };

  util::CFRef<CFMutableDictionaryRef> decoder_config = NewDict();
  CFDictionarySetValue(
      decoder_config,
      kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder,
      kCFBooleanTrue);
  CFDictionarySetValue(
      decoder_config,
      kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
      GetExtraData(codec, profile));

  CMFormatDescriptionRef cm_fmt_desc;
  if (CMVideoFormatDescriptionCreate(kCFAllocatorDefault, codec, width, height,
                                     decoder_config, &cm_fmt_desc) != 0) {
    return false;
  }

  // Create a dictionary of buffer properties.
  util::CFRef<CFNumberRef> w =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &width);
  util::CFRef<CFNumberRef> h =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &height);
  const OSType pix_fmt = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
  util::CFRef<CFNumberRef> cv_pix_fmt =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pix_fmt);
  util::CFRef<CFMutableDictionaryRef> buffer_attributes = NewDict();
  util::CFRef<CFMutableDictionaryRef> io_surface_properties = NewDict();
  CFDictionarySetValue(buffer_attributes, kCVPixelBufferPixelFormatTypeKey,
                       cv_pix_fmt);
  CFDictionarySetValue(buffer_attributes, kCVPixelBufferIOSurfacePropertiesKey,
                       io_surface_properties);
  CFDictionarySetValue(buffer_attributes, kCVPixelBufferWidthKey, w);
  CFDictionarySetValue(buffer_attributes, kCVPixelBufferHeightKey, h);


  // Try to create the decompression session, which will tell us whether it
  // supports these settings.
  VTDecompressionOutputCallbackRecord decoder_cb = {
      .decompressionOutputCallback = &IosDecoderCallback,
      .decompressionOutputRefCon = nullptr,
  };
  VTDecompressionSessionRef session = nullptr;
  const OSStatus status =
      VTDecompressionSessionCreate(nullptr, cm_fmt_desc, decoder_config,
                                   buffer_attributes, &decoder_cb, &session);
  CFRelease(cm_fmt_desc);
  if (session)
    CFRelease(session);

  switch (status) {
    case 0:
      return true;

    case kVTCouldNotFindVideoDecoderErr:
    case kVTVideoDecoderNotAvailableNowErr:
      LOG(INFO) << "Hardware decoder not available";
      return false;
    case kVTVideoDecoderUnsupportedDataFormatErr:
      VLOG(1) << "Video not supported: size=" << width << "x" << height
              << ", profile=" << std::hex << profile;
      return false;
    default:
      LOG(DFATAL) << "Bad hardware acceleration query: status=" << status;
      return false;
  }
}

#endif

bool InternalHardwareSupport(const std::string& codec, int width, int height) {
#ifdef OS_IOS
  if (codec.substr(0, 5) == "avc1.") {
    long profile = strtol(codec.substr(5).c_str(), nullptr, 16);  // NOLINT
    if (profile == 0)
      profile = 0x42001e;
    return IosHardwareSupport(kCMVideoCodecType_H264, profile, width, height);
  } else if (codec.substr(0, 5) == "mp4a.") {
    return true;
  } else {
    LOG(ERROR) << "Unable to query support for codec: " << codec;
    return false;
  }
#else
#  error "This platform doesn't have a unique hardware decoder to query."
#endif
}

}  // namespace

bool DoesHardwareSupportCodec(const std::string& codec, int width, int height) {
  if (width == 0)
    width = 720;
  if (height == 0)
    height = 480;

  BEGIN_ALLOW_COMPLEX_STATICS
  static SupportCache cache_;
  END_ALLOW_COMPLEX_STATICS
  bool result;
  if (!cache_.TryGet(codec, width, height, &result)) {
    result = InternalHardwareSupport(codec, width, height);
    cache_.Insert(codec, width, height, result);
  }
  return result;
}

}  // namespace media
}  // namespace shaka
