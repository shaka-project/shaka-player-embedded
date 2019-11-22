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

#include "src/media/ffmpeg/ffmpeg_decoder.h"

#include <glog/logging.h>

#include <string>
#include <unordered_map>

#include "src/media/ffmpeg/ffmpeg_decoded_frame.h"
#include "src/media/hardware_support.h"
#include "src/media/media_utils.h"
#include "src/util/utils.h"

namespace shaka {
namespace media {
namespace ffmpeg {

namespace {

#define LogError(code) LOG(ERROR) << "Error from FFmpeg: " << av_err2str(code)

const AVCodec* FindCodec(const std::string& codec_name) {
#ifdef ENABLE_HARDWARE_DECODE
  const AVCodec* hybrid = nullptr;
  const AVCodec* external = nullptr;
  void* opaque = nullptr;
  while (const AVCodec* codec = av_codec_iterate(&opaque)) {
    if (avcodec_get_name(codec->id) == codec_name &&
        av_codec_is_decoder(codec)) {
      if (codec->capabilities & AV_CODEC_CAP_HARDWARE)
        return codec;

      if (codec->capabilities & AV_CODEC_CAP_HYBRID) {
        // Keep the hybrid as a fallback, but try to find a hardware-only one.
        hybrid = codec;
      } else if (codec->wrapper_name) {
        // This is an external codec, which may be provided by the OS.  Fallback
        // to this if nothing else.
        external = codec;
      }
    }
  }
  if (hybrid)
    return hybrid;
  if (external)
    return external;
#endif
  return avcodec_find_decoder_by_name(codec_name.c_str());
}

std::string GetCodecFromMime(const std::string& mime) {
  std::unordered_map<std::string, std::string> params;
  if (!ParseMimeType(mime, nullptr, nullptr, &params))
    return "";
  auto it = params.find(kCodecMimeParam);
  return it != params.end() ? it->second : "";
}

}  // namespace

FFmpegDecoder::FFmpegDecoder()
    : mutex_("FFmpegDecoder"),
      decoder_ctx_(nullptr),
      received_frame_(nullptr),
#ifdef ENABLE_HARDWARE_DECODE
      hw_device_ctx_(nullptr),
      hw_pix_fmt_(AV_PIX_FMT_NONE),
#endif
      prev_timestamp_offset_(0) {
}

FFmpegDecoder::~FFmpegDecoder() {
  // It is safe if these fields are nullptr.
  avcodec_free_context(&decoder_ctx_);
  av_frame_free(&received_frame_);
#ifdef ENABLE_HARDWARE_DECODE
  av_buffer_unref(&hw_device_ctx_);
#endif
}

MediaCapabilitiesInfo FFmpegDecoder::DecodingInfo(
    const MediaDecodingConfiguration& config) const {
  MediaCapabilitiesInfo ret;
  const bool has_video = !config.video.content_type.empty();
  const bool has_audio = !config.audio.content_type.empty();
  if (has_audio == has_video || config.type != MediaDecodingType::MediaSource)
    return ret;

  const std::string codec = GetCodecFromMime(
      has_video ? config.video.content_type : config.audio.content_type);
#ifdef FORCE_HARDWARE_DECODE
  if (has_video) {
    const bool supported = DoesHardwareSupportCodec(codec, config.video.width,
                                                    config.video.height);
    ret.supported = ret.power_efficient = ret.smooth = supported;
    return ret;
  }
#endif

  auto* c = FindCodec(NormalizeCodec(codec));
  ret.supported = c != nullptr;
  ret.power_efficient = ret.smooth = c && c->wrapper_name;
  return ret;
}

void FFmpegDecoder::ResetDecoder() {
  std::unique_lock<Mutex> lock(mutex_);
  avcodec_free_context(&decoder_ctx_);
}

MediaStatus FFmpegDecoder::Decode(
    std::shared_ptr<EncodedFrame> input, const eme::Implementation* eme,
    std::vector<std::shared_ptr<DecodedFrame>>* frames) {
  std::unique_lock<Mutex> lock(mutex_);
  if (!input && !decoder_ctx_) {
    // If there isn't a decoder, there is nothing to flush.
    return MediaStatus::Success;
  }

  if (input) {
    if (!decoder_ctx_ || input->stream_info != decoder_stream_info_) {
      VLOG(1) << "Reconfiguring decoder";
      // Flush the old decoder to get any existing frames.
      if (decoder_ctx_) {
        const int send_code = avcodec_send_packet(decoder_ctx_, nullptr);
        if (send_code != 0) {
          LogError(send_code);
          return MediaStatus::FatalError;
        }
        if (!ReadFromDecoder(decoder_stream_info_, nullptr, frames))
          return MediaStatus::FatalError;
      }

      if (!InitializeDecoder(input->stream_info, true))
        return MediaStatus::FatalError;
    }

    prev_timestamp_offset_ = input->timestamp_offset;
  }


  // If the encoded frame is encrypted, decrypt it first.
  AVPacket packet{};
  util::Finally free_decrypted_packet(std::bind(&av_packet_unref, &packet));
  if (input && input->is_encrypted) {
    if (!eme) {
      LOG(WARNING) << "No CDM given for encrypted frame";
      return MediaStatus::KeyNotFound;
    }

    int code = av_new_packet(&packet, input->data_size);
    if (code < 0) {
      LogError(code);
      return MediaStatus::FatalError;
    }

    MediaStatus decrypt_status = input->Decrypt(eme, packet.data);
    if (decrypt_status == MediaStatus::KeyNotFound)
      return MediaStatus::KeyNotFound;
    if (decrypt_status != MediaStatus::Success)
      return MediaStatus::FatalError;
  } else if (input) {
    const double timescale = input->stream_info->time_scale;
    packet.pts = static_cast<int64_t>(input->pts / timescale);
    packet.dts = static_cast<int64_t>(input->dts / timescale);
    packet.data = const_cast<uint8_t*>(input->data);
    packet.size = input->data_size;
  }

  bool sent_frame = false;
  while (!sent_frame) {
    // If we get EAGAIN, we should read some frames and try to send again.
    const int send_code = avcodec_send_packet(decoder_ctx_, &packet);
    if (send_code == 0) {
      sent_frame = true;
    } else if (send_code == AVERROR_EOF) {
      // If we get EOF, this is either a flush or we are closing.  Either way,
      // stop.  If this is a flush, we can't reuse the decoder, so reset it.
      avcodec_free_context(&decoder_ctx_);
      break;
    } else if (send_code != AVERROR(EAGAIN)) {
      LogError(send_code);
      return MediaStatus::FatalError;
    }

    auto stream_info = input ? input->stream_info : decoder_stream_info_;
    if (!ReadFromDecoder(stream_info, input, frames))
      return MediaStatus::FatalError;
  }

  return MediaStatus::Success;
}

#ifdef ENABLE_HARDWARE_DECODE
AVPixelFormat FFmpegDecoder::GetPixelFormat(AVCodecContext* ctx,
                                            const AVPixelFormat* formats) {
  AVPixelFormat desired =
      reinterpret_cast<FFmpegDecoder*>(ctx->opaque)->hw_pix_fmt_;
  for (size_t i = 0; formats[i] != AV_PIX_FMT_NONE; i++) {
    if (formats[i] == desired)
      return formats[i];
  }
#  ifdef FORCE_HARDWARE_DECODE
  LOG(DFATAL) << "Hardware pixel format is unsupported.";
  return AV_PIX_FMT_NONE;
#  else
  LOG(ERROR) << "Hardware pixel format is unsupported, may be falling back "
                "to software decoder.";
  return formats[0];
#  endif
}
#endif

bool FFmpegDecoder::InitializeDecoder(std::shared_ptr<const StreamInfo> info,
                                      bool allow_hardware) {
  const AVCodec* decoder =
      allow_hardware
          ? FindCodec(NormalizeCodec(info->codec))
          : avcodec_find_decoder_by_name(NormalizeCodec(info->codec).c_str());
  CHECK(decoder) << "Should have checked support already";
#ifdef ENABLE_HARDWARE_DECODE
  AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;
  hw_pix_fmt_ = AV_PIX_FMT_NONE;
  if (allow_hardware) {
    for (int i = 0;; i++) {
      const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, i);
      if (!config) {
#  ifdef FORCE_HARDWARE_DECODE
        if (!decoder->wrapper_name) {
          LOG(DFATAL) << "No hardware-accelerators available for codec: "
                      << info->codec;
          return false;
        }
#  endif
        LOG(INFO) << "No hardware-accelerators available, using decoder: "
                  << decoder->name;
        break;
      }

      if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) {
        LOG(INFO) << "Using decoder: " << decoder->name
                  << ", with hardware accelerator: "
                  << av_hwdevice_get_type_name(config->device_type);
        hw_type = config->device_type;
        hw_pix_fmt_ = config->pix_fmt;
        break;
      }
    }
  }
#endif

  avcodec_free_context(&decoder_ctx_);
  decoder_ctx_ = avcodec_alloc_context3(decoder);
  if (!decoder_ctx_)
    return false;

  if (!received_frame_) {
    received_frame_ = av_frame_alloc();
    if (!received_frame_)
      return false;
  }

  decoder_ctx_->thread_count = 0;  // Default is 1; 0 means auto-detect.
  decoder_ctx_->opaque = this;
  decoder_ctx_->pkt_timebase = {.num = info->time_scale.numerator,
                                .den = info->time_scale.denominator};

  if (!info->extra_data.empty()) {
    av_freep(&decoder_ctx_->extradata);
    decoder_ctx_->extradata = reinterpret_cast<uint8_t*>(
        av_mallocz(info->extra_data.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!decoder_ctx_->extradata)
      return false;
    memcpy(decoder_ctx_->extradata, info->extra_data.data(),
           info->extra_data.size());
    decoder_ctx_->extradata_size = info->extra_data.size();
  }

#ifdef ENABLE_HARDWARE_DECODE
  // If using a hardware accelerator, initialize it now.
  av_buffer_unref(&hw_device_ctx_);
  if (allow_hardware && hw_type != AV_HWDEVICE_TYPE_NONE) {
    const int hw_device_code =
        av_hwdevice_ctx_create(&hw_device_ctx_, hw_type, nullptr, nullptr, 0);
    if (hw_device_code < 0) {
      LogError(hw_device_code);
      return false;
    }
    decoder_ctx_->get_format = &GetPixelFormat;
    decoder_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
  }
#endif

  const int open_code = avcodec_open2(decoder_ctx_, decoder, nullptr);
  if (open_code < 0) {
    if (open_code == AVERROR(ENOMEM))
      return false;
#if defined(ENABLE_HARDWARE_DECODE) && !defined(FORCE_HARDWARE_DECODE)
    if (allow_hardware) {
      LOG(WARNING) << "Failed to initialize hardware decoder, falling back "
                      "to software.";
      return InitializeDecoder(info, false);
    }
#endif

    LogError(open_code);
    return false;
  }

  decoder_stream_info_ = info;
  return true;
}

bool FFmpegDecoder::ReadFromDecoder(
    std::shared_ptr<const StreamInfo> stream_info,
    std::shared_ptr<EncodedFrame> input,
    std::vector<std::shared_ptr<DecodedFrame>>* decoded) {
  while (true) {
    const int code = avcodec_receive_frame(decoder_ctx_, received_frame_);
    if (code == AVERROR(EAGAIN) || code == AVERROR_EOF)
      return true;
    if (code < 0) {
      LogError(code);
      return false;
    }

    const double timescale = stream_info->time_scale;
    const int64_t timestamp = received_frame_->best_effort_timestamp;
    const double offset =
        input ? input->timestamp_offset : prev_timestamp_offset_;
    const double time = input && timestamp == AV_NOPTS_VALUE
                            ? input->pts
                            : timestamp * timescale + offset;
    auto* new_frame = FFmpegDecodedFrame::CreateFrame(
        decoder_ctx_->codec_type == AVMEDIA_TYPE_VIDEO, received_frame_, time,
        input ? input->duration : 0);
    if (!new_frame)
      return false;
    decoded->emplace_back(new_frame);
  }
}

}  // namespace ffmpeg
}  // namespace media
}  // namespace shaka
