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

#include "src/media/ios/ios_decoder.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "src/media/ios/ios_decoded_frame.h"
#include "src/media/media_utils.h"
#include "src/util/dynamic_buffer.h"

#ifndef kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder
#  define kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder \
    CFSTR("EnableHardwareAcceleratedVideoDecoder")
#endif
#ifndef kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder
#  define kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder \
    CFSTR("UsingHardwareAcceleratedVideoDecoder")
#endif

#define DEFAULT(val, def) ((val) != 0 ? (val) : (def))

namespace shaka {
namespace media {
namespace ios {

namespace {

/** The number of samples to read per chunk. */
constexpr const size_t kAudioSampleCount = 256;

/**
 * The sample format to use.  Must be packed and must match the sample size
 * below.
 */
constexpr const SampleFormat kAudioSampleFormat = SampleFormat::PackedS16;

/** The number of bytes per sample. */
constexpr const size_t kAudioSampleSize = 2;

/** The error to return when there is no more data. */
constexpr const OSStatus kNoMoreDataError = -12345;


util::CFRef<CFMutableDictionaryRef> MakeDict(size_t capacity) {
  return CFDictionaryCreateMutable(kCFAllocatorDefault, capacity,
                                   &kCFTypeDictionaryKeyCallBacks,
                                   &kCFTypeDictionaryValueCallBacks);
}

/** Creates a new buffer with a copy of the given data. */
util::CFRef<CFDataRef> CreateBuffer(const std::vector<uint8_t>& buffer) {
  return util::CFRef<CFDataRef>(
      CFDataCreate(kCFAllocatorDefault, buffer.data(), buffer.size()));
}

util::CFRef<CFMutableDictionaryRef> CreateVideoDecoderConfig(
    const std::string& codec, const std::vector<uint8_t>& extra_data) {
  util::CFRef<CFMutableDictionaryRef> ret(MakeDict(2));
  CFDictionarySetValue(
      ret, kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder,
      kCFBooleanTrue);

  const std::string raw_codec = codec.substr(0, codec.find('.'));
  util::CFRef<CFMutableDictionaryRef> info(MakeDict(1));
  if (raw_codec == "avc1" || raw_codec == "h264") {
    CFDictionarySetValue(info, CFSTR("avcC"), CreateBuffer(extra_data));
  } else if (raw_codec == "hevc") {
    CFDictionarySetValue(info, CFSTR("hvcC"), CreateBuffer(extra_data));
  } else {
    return nullptr;
  }
  CFDictionarySetValue(
      ret, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, info);

  return ret;
}

util::CFRef<CMVideoFormatDescriptionRef> CreateFormatDescription(
    const std::string& codec, uint32_t width, uint32_t height,
    util::CFRef<CFDictionaryRef> decoder_config) {
  const std::string raw_codec = codec.substr(0, codec.find('.'));
  CMVideoCodecType codec_type;
  if (raw_codec == "avc1" || raw_codec == "h264") {
    codec_type = kCMVideoCodecType_H264;
  } else if (raw_codec == "hevc") {
    codec_type = kCMVideoCodecType_HEVC;
  } else {
    return nullptr;
  }

  CMVideoFormatDescriptionRef ret;
  const auto status = CMVideoFormatDescriptionCreate(
      kCFAllocatorDefault, codec_type, width, height, decoder_config, &ret);
  if (status != 0) {
    LOG(ERROR) << "Error creating video format description: " << status;
    return nullptr;
  }

  return ret;
}

util::CFRef<CMSampleBufferRef> CreateSampleBuffer(
    util::CFRef<CMVideoFormatDescriptionRef> format_desc, const uint8_t* data,
    size_t size) {
  CMBlockBufferRef block = nullptr;
  CMSampleBufferRef ret = nullptr;
  const auto status = CMBlockBufferCreateWithMemoryBlock(
      kCFAllocatorDefault, const_cast<uint8_t*>(data), size, kCFAllocatorNull,
      nullptr, 0, size, 0, &block);
  if (status == 0) {
    CMSampleBufferCreate(kCFAllocatorDefault,  // allocator
                         block,                // dataBuffer
                         TRUE,                 // dataReady
                         nullptr,              // makeDataReadyCallback
                         nullptr,              // makeDataReadyRefcon
                         format_desc,          // formatDescription
                         1,                    // numSamples
                         0,                    // numSampleTimingEntries
                         nullptr,              // sampleTimingArray
                         0,                    // numSampleSizeEntries
                         nullptr,              // sampleSizeArray
                         &ret);                // sampleBufferOut
  }

  if (block)
    CFRelease(block);
  return ret;
}

util::CFRef<CFMutableDictionaryRef> CreateBufferAttributes(int32_t width,
                                                           int32_t height) {
  util::CFRef<CFMutableDictionaryRef> ret(MakeDict(5));
  util::CFRef<CFMutableDictionaryRef> surface_props(MakeDict(0));

  util::CFRef<CFNumberRef> w(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &width));
  util::CFRef<CFNumberRef> h(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &height));
  int32_t pix_fmt_raw = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
  util::CFRef<CFNumberRef> pix_fmt(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pix_fmt_raw));

  CFDictionarySetValue(ret, kCVPixelBufferWidthKey, w);
  CFDictionarySetValue(ret, kCVPixelBufferHeightKey, h);
  CFDictionarySetValue(ret, kCVPixelBufferPixelFormatTypeKey, pix_fmt);
  CFDictionarySetValue(ret, kCVPixelBufferIOSurfacePropertiesKey,
                       surface_props);
  CFDictionarySetValue(ret, kCVPixelBufferOpenGLESCompatibilityKey,
                       kCFBooleanTrue);

  return ret;
}

std::vector<uint8_t> MakeH264ExtraData(const std::string& codec) {
  long profile = strtol(codec.substr(5).c_str(), nullptr, 16);  // NOLINT
  if (profile == 0)
    profile = 0x42001e;

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
  return {extra_data, extra_data + sizeof(extra_data)};
}

std::vector<uint8_t> MakeAacExtraData(const std::vector<uint8_t>& codec_data) {
  // This is an ES_Descriptor box from ISO/IEC 14496-1 Section 7.2.6.5.
  constexpr const size_t kDescPrefixSize = 8;
  constexpr const size_t kConfigPrefixSize = 23;
  const size_t config_size = codec_data.size() + kConfigPrefixSize;
  const size_t total_size = config_size + kDescPrefixSize;
  CHECK_LT(total_size, 1 << (7 * 4));
  const uint8_t fixed[] = {
      // clang-format off
      0x3,  // tag=ES_DescTag
      0x80 | ((total_size >> (7 * 3)) & 0x7f),  // Data size.
      0x80 | ((total_size >> (7 * 2)) & 0x7f),
      0x80 | ((total_size >> (7 * 1)) & 0x7f),
      (total_size & 0x7f),
      0x0, 0x0,  // ES_ID
      0x0,  // Flags

      // DecoderConfigDescriptor
      0x4,  // tag=DecoderConfigDescrTag
      0x80 | ((config_size >> (7 * 3)) & 0x7f),  // Data size.
      0x80 | ((config_size >> (7 * 2)) & 0x7f),
      0x80 | ((config_size >> (7 * 1)) & 0x7f),
      (config_size & 0x7f),
      0x40,  // objectTypeIndication
      0x15,  // flags=(AudioStream)
      0x0, 0x0, 0x0,  // bufferSizeDB
      0x0, 0x0, 0x0, 0x0,  // maxBitrate
      0x0, 0x0, 0x0, 0x0,  // avgBitrate

      // DecoderSpecificInfo
      0x5,  // tag=DecSpecificInfoTag
      0x80 | ((codec_data.size() >> (7 * 3)) & 0x7f),  // Data size.
      0x80 | ((codec_data.size() >> (7 * 2)) & 0x7f),
      0x80 | ((codec_data.size() >> (7 * 1)) & 0x7f),
      (codec_data.size() & 0x7f),
      // codec_data goes here.
      // clang-format on
  };
  static_assert(kDescPrefixSize + kConfigPrefixSize == sizeof(fixed),
                "Inconsistent buffer sizes");
  DCHECK_EQ(total_size, sizeof(fixed) + codec_data.size());

  std::vector<uint8_t> ret(total_size);
  std::memcpy(ret.data(), fixed, sizeof(fixed));
  std::memcpy(ret.data() + sizeof(fixed), codec_data.data(), codec_data.size());
  return ret;
}

OSStatus CreateAudioConverter(uint32_t sample_rate, uint32_t channel_count,
                              const std::vector<uint8_t>& extra_data,
                              AudioConverterRef* session) {
  // See this for some of the magic numbers below:
  // https://developer.apple.com/documentation/coreaudiotypes/audiostreambasicdescription
  AudioStreamBasicDescription input = {0};
  input.mFormatID = kAudioFormatMPEG4AAC;
  if (extra_data.empty()) {
    // Fill in some defaults if we don't have extra data.
    input.mSampleRate = sample_rate;
    input.mChannelsPerFrame = channel_count;
    input.mBytesPerPacket = 0;  // Variable sized
    input.mFramesPerPacket = 1024;
  } else {
    // Parse the extra data to fill in "input".
    std::vector<uint8_t> cookie = MakeAacExtraData(extra_data);
    UInt32 size = sizeof(input);
    const auto status =
        AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, cookie.size(),
                               cookie.data(), &size, &input);
    if (status != 0)
      return status;
  }

  AudioStreamBasicDescription output = {0};
  output.mFormatID = kAudioFormatLinearPCM;
  output.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
  output.mSampleRate = sample_rate;
  output.mChannelsPerFrame = channel_count;
  output.mFramesPerPacket = 1;
  output.mBitsPerChannel = kAudioSampleSize * 8;
  output.mBytesPerFrame = output.mBitsPerChannel * output.mChannelsPerFrame / 8;
  output.mBytesPerPacket = output.mBytesPerFrame * output.mFramesPerPacket;

  return AudioConverterNew(&input, &output, session);
}

}  // namespace

IosDecoder::IosDecoder()
    : mutex_("IosDecoder"), at_session_(nullptr, &AudioConverterDispose) {}
IosDecoder::~IosDecoder() {
  ResetDecoder();
}

MediaCapabilitiesInfo IosDecoder::DecodingInfo(
    const MediaDecodingConfiguration& config) const {
  if (config.video.content_type.empty() == config.audio.content_type.empty())
    return MediaCapabilitiesInfo();

  const std::string& content_type = !config.video.content_type.empty()
                                        ? config.video.content_type
                                        : config.audio.content_type;
  std::unordered_map<std::string, std::string> args;
  if (!ParseMimeType(content_type, nullptr, nullptr, &args))
    return MediaCapabilitiesInfo();

  const std::string codec = args[kCodecMimeParam];
  MediaCapabilitiesInfo ret;
  if (codec.empty()) {
    // No codec, assume we can play it.
    ret.supported = true;
    return ret;
  }

  const std::string norm_codec = NormalizeCodec(codec);
  if (norm_codec == "h264") {
    const uint32_t width = DEFAULT(config.video.width, 640);
    const uint32_t height = DEFAULT(config.video.height, 480);
    const std::vector<uint8_t> extra_data = MakeH264ExtraData(codec);

    auto resolution = GetScreenResolution();
    const size_t max_size = std::max(resolution.first, resolution.second);
    if (width > max_size || height > max_size) {
      // Don't play content that is larger than the screen.  This is inefficient
      // and VideoToolbox doesn't handle out of memory correctly and has a
      // tendency to just crash with a memory error if we run out of memory.
      return ret;
    }

    VTDecompressionOutputCallbackRecord cb = {
        .decompressionOutputCallback = &IosDecoder::OnNewVideoFrame,
        .decompressionOutputRefCon = nullptr,
    };
    util::CFRef<CFDictionaryRef> decoder_config =
        CreateVideoDecoderConfig(codec, extra_data);
    auto format_desc =
        CreateFormatDescription(codec, width, height, decoder_config);
    util::CFRef<CFDictionaryRef> buffer_attr =
        CreateBufferAttributes(width, height);

    VTDecompressionSessionRef session;
    auto status = VTDecompressionSessionCreate(kCFAllocatorDefault, format_desc,
                                               decoder_config, buffer_attr, &cb,
                                               &session);

    if (status == 0) {
      ret.supported = true;

      CFBooleanRef using_hardware;
      status = VTSessionCopyProperty(
          session,
          kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder,
          kCFAllocatorDefault, &using_hardware);
      ret.smooth = ret.power_efficient =
          status == 0 && CFBooleanGetValue(using_hardware);

      VTDecompressionSessionInvalidate(session);
      CFRelease(session);
    }
  } else if (norm_codec == "aac") {
    AudioConverterRef audio_session = nullptr;
    const uint32_t sample_rate = DEFAULT(config.audio.samplerate, 44000);
    const uint32_t channel_count = DEFAULT(config.audio.channels, 2);
    const auto status =
        CreateAudioConverter(sample_rate, channel_count, {}, &audio_session);

    if (status == 0) {
      AudioConverterDispose(audio_session);
      ret.supported = ret.smooth = ret.power_efficient = true;
    }
  } else {
    // Fields are already false, unsupported.
  }

  return ret;
}

void IosDecoder::ResetDecoder() {
  std::unique_lock<Mutex> lock(mutex_);
  ResetInternal();
}

MediaStatus IosDecoder::Decode(
    std::shared_ptr<EncodedFrame> input, const eme::Implementation* eme,
    std::vector<std::shared_ptr<DecodedFrame>>* frames,
    std::string* extra_info) {
  std::unique_lock<Mutex> lock(mutex_);

  if (!input) {
    // Flush the decoder.
    bool ret;
    output_ = frames;
    if (vt_session_)
      ret = DecodeVideo(nullptr, 0, extra_info);
    else if (at_session_)
      ret = DecodeAudio(nullptr, 0, extra_info);
    else
      ret = true;

    output_ = nullptr;
    ResetInternal();  // Cannot re-use decoder after flush.
    return ret ? MediaStatus::Success : MediaStatus::FatalError;
  }

  const bool is_video = input->stream_info->is_video;
  auto init =
      is_video ? &IosDecoder::InitVideoDecoder : &IosDecoder::InitAudioDecoder;
  auto decode = is_video ? &IosDecoder::DecodeVideo : &IosDecoder::DecodeAudio;
  auto has_session = is_video ? !!vt_session_ : !!at_session_;

  if (!has_session || input->stream_info != decoder_stream_info_) {
    ResetInternal();
    if (!(this->*init)(input->stream_info, extra_info))
      return MediaStatus::FatalError;
    decoder_stream_info_ = input->stream_info;
  }

  const uint8_t* data;
  const size_t size = input->data_size;
  std::vector<uint8_t> decrypted_data;
  if (input->is_encrypted) {
    decrypted_data.resize(input->data_size);
    const auto status = input->Decrypt(eme, decrypted_data.data());
    if (status != MediaStatus::Success) {
      *extra_info = "Error decrypting frame";
      return status;
    }
    data = decrypted_data.data();
  } else {
    data = input->data;
  }

  // Store the important info in fields since we get callbacks and only get one
  // pointer for user data (this).
  input_ = input.get();
  input_data_ = data;
  input_data_size_ = size;
  output_ = frames;
  const bool ret = (this->*decode)(data, size, extra_info);
  input_ = nullptr;
  input_data_ = nullptr;
  input_data_size_ = 0;
  output_ = nullptr;
  return ret ? MediaStatus::Success : MediaStatus::FatalError;
}

void IosDecoder::OnNewVideoFrame(void* user, void* frameUser, OSStatus status,
                                 VTDecodeInfoFlags /* flags */,
                                 CVImageBufferRef buffer, CMTime pts,
                                 CMTime duration) {
  auto* decoder = reinterpret_cast<IosDecoder*>(user);
  auto* frame = decoder->input_;
  if (status != 0)
    return;

  CHECK(decoder->output_);
  CHECK(buffer);

  double time;
  if (pts.flags & kCMTimeFlags_Valid)
    time = CMTimeGetSeconds(pts);
  else
    time = frame ? frame->pts : 0;
  double durationSec;
  if (duration.flags & kCMTimeFlags_Valid)
    durationSec = CMTimeGetSeconds(duration);
  else
    durationSec = frame ? frame->duration : 0;

  decoder->output_->emplace_back(new IosDecodedFrame(
      decoder->decoder_stream_info_, time, durationSec, buffer));
}

OSStatus IosDecoder::AudioInputCallback(AudioConverterRef /* conv */,
                                        UInt32* num_packets,
                                        AudioBufferList* data,
                                        AudioStreamPacketDescription** desc,
                                        void* user) {
  auto* decoder = reinterpret_cast<IosDecoder*>(user);
  if (!decoder->input_) {
    *num_packets = 0;
    return kNoMoreDataError;
  }

  if (desc) {
    decoder->audio_desc_.mStartOffset = 0;
    decoder->audio_desc_.mVariableFramesInPacket = 0;
    decoder->audio_desc_.mDataByteSize = decoder->input_data_size_;
    *desc = &decoder->audio_desc_;
  }

  CHECK_EQ(data->mNumberBuffers, 1);
  *num_packets = 1;
  data->mBuffers[0].mNumberChannels =
      decoder->decoder_stream_info_->channel_count;
  data->mBuffers[0].mDataByteSize = decoder->input_data_size_;
  data->mBuffers[0].mData = const_cast<uint8_t*>(decoder->input_data_);

  decoder->input_ = nullptr;
  return 0;
}

void IosDecoder::ResetInternal() {
  if (vt_session_) {
    VTDecompressionSessionInvalidate(vt_session_);
    vt_session_ = nullptr;
  }
  at_session_.reset();
}

bool IosDecoder::DecodeVideo(const uint8_t* data, size_t data_size,
                             std::string* extra_info) {
  OSStatus status;
  if (data) {
    util::CFRef<CMSampleBufferRef> sample =
        CreateSampleBuffer(format_desc_, data, data_size);
    if (!sample) {
      *extra_info = "Error creating sample buffer";
      return false;
    }

    status = VTDecompressionSessionDecodeFrame(
        vt_session_, sample, kVTDecodeFrame_EnableTemporalProcessing, nullptr,
        nullptr);
    if (status == 0)
      status = VTDecompressionSessionWaitForAsynchronousFrames(vt_session_);
  } else {
    status = VTDecompressionSessionFinishDelayedFrames(vt_session_);
    if (status == 0)
      status = VTDecompressionSessionWaitForAsynchronousFrames(vt_session_);
  }

  if (status != 0) {
    LOG(ERROR) << (*extra_info =
                       "Error decoding frames: " + std::to_string(status));
    return false;
  }
  return true;
}

bool IosDecoder::DecodeAudio(const uint8_t* data, size_t data_size,
                             std::string* extra_info) {
  if (!data)
    return true;

  const size_t channel_count = decoder_stream_info_->channel_count;
  util::DynamicBuffer out_buffer;
  std::vector<uint8_t> temp_buffer;
  temp_buffer.resize(kAudioSampleCount * kAudioSampleSize * channel_count);
  auto* input = input_;

  OSStatus status = 0;
  while (status == 0) {
    AudioBufferList output{};
    output.mNumberBuffers = 1;
    output.mBuffers[0].mNumberChannels = channel_count;
    output.mBuffers[0].mDataByteSize = temp_buffer.size();
    output.mBuffers[0].mData = temp_buffer.data();
    UInt32 output_size = kAudioSampleCount;

    status = AudioConverterFillComplexBuffer(
        at_session_.get(), &IosDecoder::AudioInputCallback, this, &output_size,
        &output, nullptr);
    if (status != 0 && status != kNoMoreDataError) {
      LOG(DFATAL) << (*extra_info =
                          "Error converting audio: " + std::to_string(status));
      return false;
    }

    out_buffer.AppendCopy(temp_buffer.data(),
                          output_size * kAudioSampleSize * channel_count);
  }

  temp_buffer.resize(out_buffer.Size());
  out_buffer.CopyDataTo(temp_buffer.data(), temp_buffer.size());
  const uint32_t sample_count =
      temp_buffer.size() / kAudioSampleSize / channel_count;
  output_->emplace_back(new IosDecodedFrame(
      decoder_stream_info_, input->pts, input->duration, kAudioSampleFormat,
      sample_count, std::move(temp_buffer)));

  return true;
}

bool IosDecoder::InitVideoDecoder(std::shared_ptr<const StreamInfo> info,
                                  std::string* extra_info) {
  VTDecompressionOutputCallbackRecord cb = {
      .decompressionOutputCallback = &IosDecoder::OnNewVideoFrame,
      .decompressionOutputRefCon = this,
  };
  util::CFRef<CFDictionaryRef> decoder_config =
      CreateVideoDecoderConfig(info->codec, info->extra_data);
  format_desc_ = CreateFormatDescription(info->codec, info->width, info->height,
                                         decoder_config);
  util::CFRef<CFDictionaryRef> buffer_attr =
      CreateBufferAttributes(info->width, info->height);

  VTDecompressionSessionRef session;
  const auto status =
      VTDecompressionSessionCreate(kCFAllocatorDefault, format_desc_,
                                   decoder_config, buffer_attr, &cb, &session);
  if (status != 0) {
    LOG(ERROR) << (*extra_info = "Error creating VideoToolbox session: " +
                                 std::to_string(status));
    return false;
  }

  vt_session_ = session;
  decoder_stream_info_ = info;
  return true;
}

bool IosDecoder::InitAudioDecoder(std::shared_ptr<const StreamInfo> info,
                                  std::string* extra_info) {
  AudioConverterRef session = nullptr;
  auto status = CreateAudioConverter(info->sample_rate, info->channel_count,
                                     info->extra_data, &session);
  if (status == 0) {
    const std::vector<uint8_t> extra_data = MakeAacExtraData(info->extra_data);
    status = AudioConverterSetProperty(session,
                                       kAudioConverterDecompressionMagicCookie,
                                       extra_data.size(), extra_data.data());
  }
  if (status != 0) {
    LOG(DFATAL) << (*extra_info = "Error creating audio converter: " +
                                  std::to_string(status));
    if (session)
      AudioConverterDispose(session);
    return false;
  }
  at_session_.reset(session);
  return true;
}

}  // namespace ios
}  // namespace media
}  // namespace shaka
