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
#include "src/debug/thread_event.h"
#include "src/media/ffmpeg_decoded_frame.h"
#include "src/media/ffmpeg_encoded_frame.h"
#include "src/media/media_utils.h"
#include "src/util/utils.h"

// Special error code added by //third_party/ffmpeg/mov.patch
#define AVERROR_SHAKA_RESET_DEMUXER (-123456)

namespace shaka {
namespace media {

namespace {

constexpr const size_t kInitialBufferSize = 2048;

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

const AVCodec* FindCodec(AVCodecID codec_id) {
#ifdef ENABLE_HARDWARE_DECODE
  const AVCodec* hybrid = nullptr;
  const AVCodec* external = nullptr;
  void* opaque = nullptr;
  while (const AVCodec* codec = av_codec_iterate(&opaque)) {
    if (codec->id == codec_id && av_codec_is_decoder(codec)) {
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
  return avcodec_find_decoder(codec_id);
}

/**
 * Creates a 'pssh' box from the given encryption info.  FFmpeg outputs the
 * encryption info in a generic structure, but EME expects it in one of several
 * binary formats.  We use the 'cenc' format, which is one or more 'pssh' boxes.
 */
std::vector<uint8_t> CreatePssh(AVEncryptionInitInfo* info) {
  // 4 box size
  // 4 box type
  // 1 version
  // 3 flags
  // 16 system_id
  // if (version > 0)
  //   4 key_id_count
  //   for (key_id_count)
  //     16 key_id
  // 4 data_size
  // [data_size] data
  DCHECK_EQ(info->system_id_size, 16u);
  size_t pssh_size = info->data_size + 32;
  if (info->num_key_ids) {
    DCHECK_EQ(info->key_id_size, 16u);
    pssh_size += 4 + info->num_key_ids * 16;
  }

#define WRITE_INT(ptr, num) *reinterpret_cast<uint32_t*>(ptr) = htonl(num)

  std::vector<uint8_t> pssh(pssh_size, 0);
  uint8_t* ptr = pssh.data();
  WRITE_INT(ptr, pssh_size);
  WRITE_INT(ptr + 4, 0x70737368);  // 'pssh'
  WRITE_INT(ptr + 8, info->num_key_ids ? 0x01000000 : 0);
  std::memcpy(ptr + 12, info->system_id, 16);
  ptr += 28;
  if (info->num_key_ids) {
    WRITE_INT(ptr, info->num_key_ids);
    ptr += 4;
    for (uint32_t i = 0; i < info->num_key_ids; i++) {
      std::memcpy(ptr, info->key_ids[i], 16);
      ptr += 16;
    }
  }

  WRITE_INT(ptr, info->data_size);
  std::memcpy(ptr + 4, info->data, info->data_size);
  DCHECK_EQ(ptr + info->data_size + 4, pssh.data() + pssh_size);

#undef WRITE_INT
  return pssh;
}

}  // namespace

class MediaProcessor::Impl {
 public:
  Impl(const std::string& container, const std::string& codec,
       std::function<void(eme::MediaKeyInitDataType, const uint8_t*, size_t)>
           on_encrypted_init_data)
      : mutex_("MediaProcessor"),
        signal_("MediaProcessor demuxer ready"),
        on_encrypted_init_data_(std::move(on_encrypted_init_data)),
        container_(NormalizeContainer(container)),
        codec_(NormalizeCodec(codec)),
        io_(nullptr),
        demuxer_ctx_(nullptr),
        decoder_ctx_(nullptr),
        received_frame_(nullptr),
#ifdef ENABLE_HARDWARE_DECODE
        hw_device_ctx_(nullptr),
        hw_pix_fmt_(AV_PIX_FMT_NONE),
#endif
        timestamp_offset_(0),
        prev_timestamp_offset_(0),
        decoder_stream_id_(0) {
  }

  ~Impl() {
    if (io_) {
      // If an IO buffer was allocated by libavformat, it must be freed by us.
      if (io_->buffer) {
        av_free(io_->buffer);
      }
      // The IO context itself must be freed by us as well.  Closing ctx_ does
      // not free the IO context attached to it.
      av_free(io_);
    }

    // It is safe if these fields are nullptr.
    for (AVCodecParameters*& params : codec_params_)
      avcodec_parameters_free(&params);
    avcodec_free_context(&decoder_ctx_);
    avformat_close_input(&demuxer_ctx_);
    av_frame_free(&received_frame_);
#ifdef ENABLE_HARDWARE_DECODE
    av_buffer_unref(&hw_device_ctx_);
#endif
  }

  NON_COPYABLE_OR_MOVABLE_TYPE(Impl);

  std::string container() const {
    return container_;
  }
  std::string codec() const {
    return codec_;
  }

  double duration() const {
    std::unique_lock<Mutex> lock(mutex_);
    if (!demuxer_ctx_ || demuxer_ctx_->duration == AV_NOPTS_VALUE)
      return 0;
    return static_cast<double>(demuxer_ctx_->duration) / AV_TIME_BASE;
  }

  Status ReinitDemuxer(std::unique_lock<Mutex>* lock) {
    avformat_close_input(&demuxer_ctx_);
    avio_flush(io_);

    AVFormatContext* new_ctx;
    // Create the new demuxer without the lock held because the method will
    // block while waiting for the init segment.
    lock->unlock();
    const Status status = CreateDemuxer(&new_ctx);
    if (status != Status::Success)
      return status;
    lock->lock();
    demuxer_ctx_ = new_ctx;

    AVStream* stream = demuxer_ctx_->streams[0];
    auto* params = avcodec_parameters_alloc();
    if (!params) {
      return Status::OutOfMemory;
    }
    const int copy_code = avcodec_parameters_copy(params, stream->codecpar);
    if (copy_code < 0) {
      avcodec_parameters_free(&params);
      HandleGenericFFmpegError(copy_code);
      return Status::CannotOpenDemuxer;
    }
    codec_params_.push_back(params);
    time_scales_.push_back(stream->time_base);

    return Status::Success;
  }

  Status CreateDemuxer(AVFormatContext** context) {
    // Allocate a demuxer context and set it to use the IO context.
    AVFormatContext* demuxer = avformat_alloc_context();
    if (!demuxer) {
      return Status::OutOfMemory;
    }
    demuxer->pb = io_;
    // If we enable the probes, in encrypted content we'll get logs about being
    // unable to parse the content; however, if we disable the probes, we won't
    // get accurate frame durations, which can cause problems.
    // TODO: Find a way to conditionally disable parsing or to suppress the
    // logs for encrypted content (since the errors there aren't fatal).
    // demuxer->probesize = 0;
    // demuxer->max_analyze_duration = 0;

    // To enable extremely verbose logging:
    // demuxer->debug = 1;

    // Parse encryption info for WebM; ignored for other demuxers.
    AVDictionary* dict = nullptr;
    CHECK_EQ(av_dict_set_int(&dict, "parse_encryption", 1, 0), 0);

    AVInputFormat* format = av_find_input_format(container_.c_str());
    CHECK(format) << "Should have checked for support before creating.";
    const int open_code = avformat_open_input(&demuxer, nullptr, format, &dict);
    av_dict_free(&dict);
    if (open_code < 0) {
      // If avformat_open_input fails, it will free and reset |demuxer|.
      DCHECK(!demuxer);

      if (open_code == AVERROR(ENOMEM))
        return Status::OutOfMemory;
      if (open_code == AVERROR_INVALIDDATA)
        return Status::InvalidContainerData;

      HandleGenericFFmpegError(open_code);
      return Status::CannotOpenDemuxer;
    }

    const int find_code = avformat_find_stream_info(demuxer, nullptr);
    if (find_code < 0) {
      avformat_close_input(&demuxer);
      if (find_code == AVERROR(ENOMEM))
        return Status::OutOfMemory;
      if (find_code == AVERROR_INVALIDDATA)
        return Status::InvalidContainerData;

      HandleGenericFFmpegError(find_code);
      return Status::CannotOpenDemuxer;
    }

    if (demuxer->nb_streams == 0) {
      avformat_close_input(&demuxer);
      return Status::NoStreamsFound;
    }
    if (demuxer->nb_streams > 1) {
      avformat_close_input(&demuxer);
      return Status::MultiplexedContentFound;
    }

    *context = demuxer;
    return Status::Success;
  }

  Status InitializeDemuxer(std::function<size_t(uint8_t*, size_t)> on_read,
                           std::function<void()> on_reset_read) {
    std::unique_lock<Mutex> lock(mutex_);

    on_read_ = std::move(on_read);
    on_reset_read_ = std::move(on_reset_read);

    // Allocate a context for custom IO.
    // NOTE: The buffer may be reallocated/resized by libavformat later.
    // It is always our responsibility to free it later with av_free.
    io_ = avio_alloc_context(
        reinterpret_cast<unsigned char*>(av_malloc(kInitialBufferSize)),
        kInitialBufferSize,
        0,     // write_flag (read-only)
        this,  // opaque user data
        &Impl::ReadCallback,
        nullptr,   // write callback (read-only)
        nullptr);  // seek callback (linear reads only)
    if (!io_) {
      return Status::OutOfMemory;
    }

    received_frame_ = av_frame_alloc();
    if (!received_frame_) {
      return Status::OutOfMemory;
    }

    const Status reinit_status = ReinitDemuxer(&lock);
    if (reinit_status == Status::Success)
      signal_.SignalAll();

    return reinit_status;
  }

  Status ReadDemuxedFrame(std::unique_ptr<BaseFrame>* frame) {
    AVPacket pkt;
    int ret = av_read_frame(demuxer_ctx_, &pkt);
    if (ret == AVERROR_SHAKA_RESET_DEMUXER) {
      // Special case for Shaka where we need to reinit the demuxer.
      VLOG(1) << "Reinitializing demuxer";
      on_reset_read_();

      std::unique_lock<Mutex> lock(mutex_);
      const Status reinit_status = ReinitDemuxer(&lock);
      if (reinit_status != Status::Success)
        return reinit_status;
      ret = av_read_frame(demuxer_ctx_, &pkt);
    }
    if (ret < 0) {
      av_packet_unref(&pkt);
      if (ret == AVERROR_EOF)
        return Status::EndOfStream;
      if (ret == AVERROR(ENOMEM))
        return Status::OutOfMemory;
      if (ret == AVERROR_INVALIDDATA)
        return Status::InvalidContainerData;

      HandleGenericFFmpegError(ret);
      return Status::UnknownError;
    }

    UpdateEncryptionInitInfo();

    // Ignore discard flags.  The demuxer will set this when we try to read
    // content behind media we have already read.
    pkt.flags &= ~AV_PKT_FLAG_DISCARD;

    VLOG(3) << "Read frame at dts=" << pkt.dts;
    DCHECK_EQ(pkt.stream_index, 0);
    DCHECK_EQ(demuxer_ctx_->nb_streams, 1u);
    frame->reset(FFmpegEncodedFrame::MakeFrame(&pkt, demuxer_ctx_->streams[0],
                                               codec_params_.size() - 1,
                                               timestamp_offset_));
    // No need to unref |pkt| since it was moved into the encoded frame.
    return *frame ? Status::Success : Status::OutOfMemory;
  }

  Status InitializeDecoder(size_t stream_id, bool allow_hardware) {
    const AVCodecParameters* params = codec_params_[stream_id];
    const char* codec_name = avcodec_get_name(params->codec_id);
    if (codec_ != codec_name) {
      LOG(ERROR) << "Mismatch between codec string and media.  Codec string: '"
                 << codec_ << "', media codec: '" << codec_name << "' (0x"
                 << std::hex << params->codec_id << ")";
      return Status::DecoderMismatch;
    }

    const AVCodec* decoder = allow_hardware
                                 ? FindCodec(params->codec_id)
                                 : avcodec_find_decoder(params->codec_id);
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

    const int param_code = avcodec_parameters_to_context(decoder_ctx_, params);
    if (param_code < 0) {
      if (param_code == AVERROR(ENOMEM)) {
        return Status::OutOfMemory;
      }

      HandleGenericFFmpegError(param_code);
      return Status::DecoderFailedInit;
    }
    decoder_ctx_->thread_count = 0;  // Default is 1; 0 means auto-detect.
    decoder_ctx_->opaque = this;
    decoder_ctx_->pkt_timebase = time_scales_[stream_id];

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
        return InitializeDecoder(stream_id, false);
      }
#endif

      HandleGenericFFmpegError(open_code);
      return Status::DecoderFailedInit;
    }

    decoder_stream_id_ = stream_id;
    return Status::Success;
  }

  Status ReadFromDecoder(size_t stream_id, const FFmpegEncodedFrame* frame,
                         std::vector<std::unique_ptr<BaseFrame>>* decoded) {
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

      const double timescale = av_q2d(time_scales_[stream_id]);
      const int64_t timestamp = received_frame_->best_effort_timestamp;
      const double offset =
          frame ? frame->timestamp_offset() : prev_timestamp_offset_;
      const double time = frame && timestamp == AV_NOPTS_VALUE
                              ? frame->pts
                              : timestamp * timescale + offset;
      auto* new_frame = FFmpegDecodedFrame::CreateFrame(
          received_frame_, time, frame ? frame->duration : 0);
      if (!new_frame) {
        return Status::OutOfMemory;
      }
      decoded->emplace_back(new_frame);
    }
  }

  Status DecodeFrame(double /* cur_time */, const BaseFrame* base_frame,
                     eme::Implementation* cdm,
                     std::vector<std::unique_ptr<BaseFrame>>* decoded) {
    decoded->clear();
    DCHECK(!base_frame ||
           base_frame->frame_type() == FrameType::FFmpegEncodedFrame);
    auto* frame = static_cast<const FFmpegEncodedFrame*>(base_frame);

    if (!frame && !decoder_ctx_) {
      // If there isn't a decoder, there is nothing to flush.
      return Status::Success;
    }

    if (frame) {
      // Wait for the signal before acquiring the lock to avoid a deadlock.
      signal_.GetValue();
      std::unique_lock<Mutex> lock(mutex_);

      if (!decoder_ctx_ || frame->stream_id() != decoder_stream_id_) {
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
              ReadFromDecoder(decoder_stream_id_, nullptr, decoded);
          if (read_result != Status::Success)
            return read_result;
        }

        const Status init_result = InitializeDecoder(frame->stream_id(), true);
        if (init_result != Status::Success)
          return init_result;
      }

      prev_timestamp_offset_ = frame->timestamp_offset();
    }


    // If the encoded frame is encrypted, decrypt it first.
    AVPacket decrypted_packet{};
    util::Finally free_decrypted_packet(
        std::bind(&av_packet_unref, &decrypted_packet));
    const AVPacket* frame_to_send = frame ? frame->raw_packet() : nullptr;
    if (frame && frame->is_encrypted()) {
      if (!cdm) {
        LOG(WARNING) << "No CDM given for encrypted frame";
        return Status::KeyNotFound;
      }

      int code = av_new_packet(&decrypted_packet, frame->raw_packet()->size);
      if (code == 0)
        code = av_packet_copy_props(&decrypted_packet, frame->raw_packet());
      if (code == AVERROR(ENOMEM))
        return Status::OutOfMemory;
      if (code < 0) {
        HandleGenericFFmpegError(code);
        return Status::UnknownError;
      }

      Status decrypt_status = frame->Decrypt(cdm, &decrypted_packet);
      if (decrypt_status != Status::Success)
        return decrypt_status;
      frame_to_send = &decrypted_packet;
    }

    bool sent_frame = false;
    while (!sent_frame) {
      // If we get EAGAIN, we should read some frames and try to send again.
      const int send_code = avcodec_send_packet(decoder_ctx_, frame_to_send);
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

      const int stream_id = frame ? frame->stream_id() : decoder_stream_id_;
      const Status read_result = ReadFromDecoder(stream_id, frame, decoded);
      if (read_result != Status::Success)
        return read_result;
    }

    return Status::Success;
  }

  void SetTimestampOffset(double offset) {
    timestamp_offset_ = offset;
  }

  void ResetDecoder() {
    avcodec_free_context(&decoder_ctx_);
  }

 private:
  static int ReadCallback(void* opaque, uint8_t* buffer, int size) {
    DCHECK_GE(size, 0);
    const size_t count =
        reinterpret_cast<Impl*>(opaque)->on_read_(buffer, size);
    return count == 0 ? AVERROR_EOF : count;
  }

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

  void UpdateEncryptionInitInfo() {
    int side_data_size;
    const uint8_t* side_data = av_stream_get_side_data(
        demuxer_ctx_->streams[0], AV_PKT_DATA_ENCRYPTION_INIT_INFO,
        &side_data_size);
    if (side_data) {
      AVEncryptionInitInfo* info =
          av_encryption_init_info_get_side_data(side_data, side_data_size);
      std::vector<uint8_t> pssh;
      for (auto* cur_info = info; cur_info; cur_info = cur_info->next) {
        if (cur_info->system_id_size) {
          const std::vector<uint8_t> temp = CreatePssh(cur_info);
          pssh.insert(pssh.end(), temp.begin(), temp.end());
        } else {
          for (size_t i = 0; i < cur_info->num_key_ids; i++) {
            on_encrypted_init_data_(eme::MediaKeyInitDataType::WebM,
                                    cur_info->key_ids[i],
                                    cur_info->key_id_size);
          }
        }
      }
      if (!pssh.empty()) {
        on_encrypted_init_data_(eme::MediaKeyInitDataType::Cenc, pssh.data(),
                                pssh.size());
      }
      av_encryption_init_info_free(info);

      av_stream_remove_side_data(demuxer_ctx_->streams[0],
                                 AV_PKT_DATA_ENCRYPTION_INIT_INFO);
    }
  }


  mutable Mutex mutex_;
  ThreadEvent<void> signal_;
  std::function<void(eme::MediaKeyInitDataType, const uint8_t*, size_t)>
      on_encrypted_init_data_;
  std::function<size_t(uint8_t*, size_t)> on_read_;
  std::function<void()> on_reset_read_;
  const std::string container_;
  const std::string codec_;

  std::vector<AVCodecParameters*> codec_params_;
  std::vector<AVRational> time_scales_;
  AVIOContext* io_;
  AVFormatContext* demuxer_ctx_;
  AVCodecContext* decoder_ctx_;
  AVFrame* received_frame_;
#ifdef ENABLE_HARDWARE_DECODE
  AVBufferRef* hw_device_ctx_;
  AVPixelFormat hw_pix_fmt_;
#endif
  double timestamp_offset_;
  double prev_timestamp_offset_;
  // The stream ID the decoder is currently configured to use.
  size_t decoder_stream_id_;
};

MediaProcessor::MediaProcessor(
    const std::string& container, const std::string& codec,
    std::function<void(eme::MediaKeyInitDataType, const uint8_t*, size_t)>
        on_encrypted_init_data)
    : impl_(new Impl(container, codec, std::move(on_encrypted_init_data))) {}
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

std::string MediaProcessor::container() const {
  return impl_->container();
}

std::string MediaProcessor::codec() const {
  return impl_->codec();
}

double MediaProcessor::duration() const {
  return impl_->duration();
}

Status MediaProcessor::InitializeDemuxer(
    std::function<size_t(uint8_t*, size_t)> on_read,
    std::function<void()> on_reset_read) {
  return impl_->InitializeDemuxer(std::move(on_read), std::move(on_reset_read));
}

Status MediaProcessor::ReadDemuxedFrame(std::unique_ptr<BaseFrame>* frame) {
  return impl_->ReadDemuxedFrame(frame);
}

Status MediaProcessor::DecodeFrame(
    double cur_time, const BaseFrame* frame, eme::Implementation* cdm,
    std::vector<std::unique_ptr<BaseFrame>>* decoded) {
  return impl_->DecodeFrame(cur_time, frame, cdm, decoded);
}

void MediaProcessor::SetTimestampOffset(double offset) {
  impl_->SetTimestampOffset(offset);
}

void MediaProcessor::ResetDecoder() {
  impl_->ResetDecoder();
}

}  // namespace media
}  // namespace shaka
