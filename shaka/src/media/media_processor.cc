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

#include "src/media/media_processor.h"

#include <netinet/in.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/encryption_info.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
}

#include <cstring>
#include <utility>

#include "src/core/js_manager_impl.h"
#include "src/debug/mutex.h"
#include "src/media/ffmpeg/ffmpeg_decoded_frame.h"
#include "src/media/ffmpeg/ffmpeg_encoded_frame.h"
#include "src/media/media_utils.h"
#include "src/util/utils.h"

namespace shaka {
namespace media {

namespace {

std::string ErrStr(int code) {
  if (code == 0)
    return "Success";

  const char* prefix = code < 0 ? "-" : "";
  return util::StringPrintf("%s0x%08x: %s", prefix, abs(code),
                            av_err2str(code));
}

/**
 * Prints logs about the given FFmpeg error code.  Many of the codes don't apply
 * to us, so this method asserts that we don't see those codes.  For those that
 * apply, this prints logs about it.
 */
void HandleGenericFFmpegError(int code) {
  // See //third_party/ffmpeg/src/libavutil/error.h
  switch (code) {
    case AVERROR_BSF_NOT_FOUND:
    case AVERROR_DECODER_NOT_FOUND:
    case AVERROR_DEMUXER_NOT_FOUND:
    case AVERROR_ENCODER_NOT_FOUND:
    case AVERROR_FILTER_NOT_FOUND:
    case AVERROR_MUXER_NOT_FOUND:
    case AVERROR_OPTION_NOT_FOUND:
    case AVERROR_STREAM_NOT_FOUND:
      // This should be handled by VideoController::AddSource.
      LOG(DFATAL) << "Unable to find media handler: " << ErrStr(code);
      break;
    case AVERROR_BUFFER_TOO_SMALL:
    case AVERROR_EOF:
    case AVERROR_INVALIDDATA:
    case AVERROR_INPUT_CHANGED:
    case AVERROR_OUTPUT_CHANGED:
    case AVERROR(EAGAIN):
    case AVERROR(EINVAL):
    case AVERROR(ENOMEM):
      // Calling code should handle these codes.
      LOG(DFATAL) << "Special error not handled: " << ErrStr(code);
      break;
    case AVERROR_HTTP_BAD_REQUEST:
    case AVERROR_HTTP_UNAUTHORIZED:
    case AVERROR_HTTP_FORBIDDEN:
    case AVERROR_HTTP_NOT_FOUND:
    case AVERROR_HTTP_OTHER_4XX:
    case AVERROR_HTTP_SERVER_ERROR:
    case AVERROR_PROTOCOL_NOT_FOUND:
      // We don't use FFmpeg's networking, so this shouldn't happen.
      LOG(DFATAL) << "Unexpected networking error: " << ErrStr(code);
      break;
    case AVERROR_BUG:
    case AVERROR_BUG2:
    case AVERROR_PATCHWELCOME:
      LOG(DFATAL) << "Bug inside FFmpeg: " << ErrStr(code);
      break;

    case AVERROR_EXIT:
    case AVERROR_EXTERNAL:
    case AVERROR_UNKNOWN:
      LOG(ERROR) << "Unknown error inside FFmpeg: " << ErrStr(code);
      break;

    default:
      LOG(DFATAL) << "Unknown error: " << ErrStr(code);
      break;
  }
}

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

}  // namespace

class MediaProcessor::Impl {
 public:
  explicit Impl(const std::string& codec)
      : mutex_("MediaProcessor"),
        codec_(NormalizeCodec(codec)),
        decoder_ctx_(nullptr),
        received_frame_(nullptr),
#ifdef ENABLE_HARDWARE_DECODE
        hw_device_ctx_(nullptr),
        hw_pix_fmt_(AV_PIX_FMT_NONE),
#endif
        prev_timestamp_offset_(0) {
  }

  ~Impl() {
    // It is safe if these fields are nullptr.
    avcodec_free_context(&decoder_ctx_);
    av_frame_free(&received_frame_);
#ifdef ENABLE_HARDWARE_DECODE
    av_buffer_unref(&hw_device_ctx_);
#endif
  }

  NON_COPYABLE_OR_MOVABLE_TYPE(Impl);

  std::string codec() const {
    return codec_;
  }

  Status InitializeDecoder(std::shared_ptr<const StreamInfo> info,
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
                        << codec_;
            return Status::DecoderFailedInit;
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
    if (!decoder_ctx_) {
      return Status::OutOfMemory;
    }

    if (!received_frame_) {
      received_frame_ = av_frame_alloc();
      if (!received_frame_)
        return Status::OutOfMemory;
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
        return Status::OutOfMemory;
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
        if (hw_device_code == AVERROR(ENOMEM)) {
          return Status::OutOfMemory;
        }

        HandleGenericFFmpegError(hw_device_code);
        return Status::DecoderFailedInit;
      }
      decoder_ctx_->get_format = &GetPixelFormat;
      decoder_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
    }
#endif

    const int open_code = avcodec_open2(decoder_ctx_, decoder, nullptr);
    if (open_code < 0) {
      if (open_code == AVERROR(ENOMEM))
        return Status::OutOfMemory;
#if defined(ENABLE_HARDWARE_DECODE) && !defined(FORCE_HARDWARE_DECODE)
      if (allow_hardware) {
        LOG(WARNING) << "Failed to initialize hardware decoder, falling back "
                        "to software.";
        return InitializeDecoder(info, false);
      }
#endif

      HandleGenericFFmpegError(open_code);
      return Status::DecoderFailedInit;
    }

    decoder_stream_info_ = info;
    return Status::Success;
  }

  Status ReadFromDecoder(std::shared_ptr<const StreamInfo> stream_info,
                         std::shared_ptr<EncodedFrame> frame,
                         std::vector<std::shared_ptr<DecodedFrame>>* decoded) {
    while (true) {
      const int code = avcodec_receive_frame(decoder_ctx_, received_frame_);
      if (code == AVERROR(EAGAIN) || code == AVERROR_EOF)
        return Status::Success;
      if (code == AVERROR_INVALIDDATA)
        return Status::InvalidCodecData;
      if (code < 0) {
        HandleGenericFFmpegError(code);
        return Status::UnknownError;
      }

      const double timescale = stream_info->time_scale;
      const int64_t timestamp = received_frame_->best_effort_timestamp;
      const double offset =
          frame ? frame->timestamp_offset : prev_timestamp_offset_;
      const double time = frame && timestamp == AV_NOPTS_VALUE
                              ? frame->pts
                              : timestamp * timescale + offset;
      auto* new_frame = ffmpeg::FFmpegDecodedFrame::CreateFrame(
          decoder_ctx_->codec_type == AVMEDIA_TYPE_VIDEO, received_frame_, time,
          frame ? frame->duration : 0);
      if (!new_frame) {
        return Status::OutOfMemory;
      }
      decoded->emplace_back(new_frame);
    }
  }

  Status DecodeFrame(double /* cur_time */, std::shared_ptr<EncodedFrame> frame,
                     eme::Implementation* cdm,
                     std::vector<std::shared_ptr<DecodedFrame>>* decoded) {
    decoded->clear();

    if (!frame && !decoder_ctx_) {
      // If there isn't a decoder, there is nothing to flush.
      return Status::Success;
    }

    if (frame) {
      std::unique_lock<Mutex> lock(mutex_);

      if (!decoder_ctx_ || frame->stream_info != decoder_stream_info_) {
        VLOG(1) << "Reconfiguring decoder";
        // Flush the old decoder to get any existing frames.
        if (decoder_ctx_) {
          const int send_code = avcodec_send_packet(decoder_ctx_, nullptr);
          if (send_code != 0) {
            if (send_code == AVERROR(ENOMEM))
              return Status::OutOfMemory;
            if (send_code == AVERROR_INVALIDDATA)
              return Status::InvalidCodecData;

            HandleGenericFFmpegError(send_code);
            return Status::UnknownError;
          }
          const Status read_result =
              ReadFromDecoder(decoder_stream_info_, nullptr, decoded);
          if (read_result != Status::Success)
            return read_result;
        }

        const Status init_result = InitializeDecoder(frame->stream_info, true);
        if (init_result != Status::Success)
          return init_result;
      }

      prev_timestamp_offset_ = frame->timestamp_offset;
    }


    // If the encoded frame is encrypted, decrypt it first.
    AVPacket packet{};
    util::Finally free_decrypted_packet(std::bind(&av_packet_unref, &packet));
    if (frame && frame->is_encrypted) {
      if (!cdm) {
        LOG(WARNING) << "No CDM given for encrypted frame";
        return Status::KeyNotFound;
      }

      int code = av_new_packet(&packet, frame->data_size);
      if (code == AVERROR(ENOMEM))
        return Status::OutOfMemory;
      if (code < 0) {
        HandleGenericFFmpegError(code);
        return Status::UnknownError;
      }

      MediaStatus decrypt_status = frame->Decrypt(cdm, packet.data);
      if (decrypt_status == MediaStatus::KeyNotFound)
        return Status::KeyNotFound;
      if (decrypt_status != MediaStatus::Success)
        return Status::UnknownError;
    } else if (frame) {
      const double timescale = frame->stream_info->time_scale;
      packet.pts = static_cast<int64_t>(frame->pts / timescale);
      packet.dts = static_cast<int64_t>(frame->dts / timescale);
      packet.data = const_cast<uint8_t*>(frame->data);
      packet.size = frame->data_size;
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
        ResetDecoder();
        break;
      } else if (send_code != AVERROR(EAGAIN)) {
        if (send_code == AVERROR(ENOMEM))
          return Status::OutOfMemory;
        if (send_code == AVERROR_INVALIDDATA)
          return Status::InvalidCodecData;

        HandleGenericFFmpegError(send_code);
        return Status::UnknownError;
      }

      auto stream_info = frame ? frame->stream_info : decoder_stream_info_;
      const Status read_result = ReadFromDecoder(stream_info, frame, decoded);
      if (read_result != Status::Success)
        return read_result;
    }

    return Status::Success;
  }

  void ResetDecoder() {
    avcodec_free_context(&decoder_ctx_);
  }

 private:
#ifdef ENABLE_HARDWARE_DECODE
  static AVPixelFormat GetPixelFormat(AVCodecContext* ctx,
                                      const AVPixelFormat* formats) {
    AVPixelFormat desired = reinterpret_cast<Impl*>(ctx->opaque)->hw_pix_fmt_;
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


  mutable Mutex mutex_;
  const std::string container_;
  const std::string codec_;

  AVCodecContext* decoder_ctx_;
  AVFrame* received_frame_;
#ifdef ENABLE_HARDWARE_DECODE
  AVBufferRef* hw_device_ctx_;
  AVPixelFormat hw_pix_fmt_;
#endif
  double prev_timestamp_offset_;
  // The stream the decoder is currently configured to use.
  std::shared_ptr<const StreamInfo> decoder_stream_info_;
};

MediaProcessor::MediaProcessor(const std::string& codec)
    : impl_(new Impl(codec)) {}
MediaProcessor::~MediaProcessor() {}

// static
void MediaProcessor::Initialize() {
  CHECK_EQ(avformat_version(),
           static_cast<unsigned int>(LIBAVFORMAT_VERSION_INT))
      << "Running against wrong shared library version!";

#ifdef NDEBUG
  av_log_set_level(AV_LOG_ERROR);
#else
  av_log_set_level(AV_LOG_VERBOSE);
#endif
}

std::string MediaProcessor::codec() const {
  return impl_->codec();
}

Status MediaProcessor::DecodeFrame(
    double cur_time, std::shared_ptr<EncodedFrame> frame,
    eme::Implementation* cdm,
    std::vector<std::shared_ptr<DecodedFrame>>* decoded) {
  return impl_->DecodeFrame(cur_time, frame, cdm, decoded);
}

void MediaProcessor::ResetDecoder() {
  impl_->ResetDecoder();
}

}  // namespace media
}  // namespace shaka
