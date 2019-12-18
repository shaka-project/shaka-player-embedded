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

#include "src/media/ffmpeg/ffmpeg_demuxer.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/encryption_info.h>
}
#include <glog/logging.h>

#include <algorithm>
#include <unordered_map>

#include "src/media/ffmpeg/ffmpeg_encoded_frame.h"
#include "src/media/media_utils.h"
#include "src/util/buffer_writer.h"

// Special error code added by //third_party/ffmpeg/mov.patch
#define AVERROR_SHAKA_RESET_DEMUXER (-123456)

namespace shaka {
namespace media {
namespace ffmpeg {

namespace {

constexpr const size_t kInitialBufferSize = 2048;

void LogError(int code) {
  LOG(ERROR) << "Error from FFmpeg: " << av_err2str(code);
}

std::string GetCodec(const std::string& mime, AVCodecID codec) {
  std::unordered_map<std::string, std::string> params;
  CHECK(ParseMimeType(mime, nullptr, nullptr, &params));
  if (params.count(kCodecMimeParam) > 0)
    return params.at(kCodecMimeParam);
  else
    return avcodec_get_name(codec);
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

  std::vector<uint8_t> pssh(pssh_size, 0);
  util::BufferWriter writer(pssh.data(), pssh_size);

  writer.Write<uint32_t>(pssh_size);
  writer.WriteTag("pssh");
  writer.Write<uint32_t>(info->num_key_ids ? 0x01000000 : 0);
  writer.Write(info->system_id, 16);
  if (info->num_key_ids) {
    writer.Write<uint32_t>(info->num_key_ids);
    for (uint32_t i = 0; i < info->num_key_ids; i++) {
      writer.Write(info->key_ids[i], 16);
    }
  }

  writer.Write<uint32_t>(info->data_size);
  writer.Write(info->data, info->data_size);
  DCHECK(writer.empty());

  return pssh;
}

bool ParseAndCheckSupport(const std::string& mime, std::string* container) {
  std::string subtype;
  if (!ParseMimeType(mime, nullptr, &subtype, nullptr))
    return false;

  std::string normalized = NormalizeContainer(subtype);
  if (!av_find_input_format(normalized.c_str()))
    return false;

  *container = normalized;
  return true;
}

}  // namespace

FFmpegDemuxer::FFmpegDemuxer(Demuxer::Client* client,
                             const std::string& mime_type,
                             const std::string& container)
    : signal_("FFmpegDemuxer"),
      mutex_("FFmpegDemuxer"),
      mime_type_(mime_type),
      container_(container),
      io_(nullptr),
      demuxer_ctx_(nullptr),
      client_(client),
      output_(nullptr),
      timestamp_offset_(0),
      input_(nullptr),
      input_size_(0),
      input_pos_(0),
      state_(State::Waiting),
      thread_("FFmepgDemuxer", std::bind(&FFmpegDemuxer::ThreadMain, this)) {}

FFmpegDemuxer::~FFmpegDemuxer() {
  {
    std::unique_lock<Mutex> lock(mutex_);
    state_ = State::Stopping;
    signal_.SignalAll();
  }
  thread_.join();

  if (io_) {
    // If an IO buffer was allocated by libavformat, it must be freed by us.
    if (io_->buffer) {
      av_free(io_->buffer);
    }
    // The IO context itself must be freed by us as well.  Closing ctx_ does
    // not free the IO context attached to it.
    av_free(io_);
  }
}

void FFmpegDemuxer::Reset() {
  // TODO: Add Reset capability.
}

bool FFmpegDemuxer::Demux(double timestamp_offset, const uint8_t* data,
                          size_t size,
                          std::vector<std::shared_ptr<EncodedFrame>>* frames) {
  std::unique_lock<Mutex> lock(mutex_);
  if (state_ != State::Waiting) {
    DCHECK(state_ == State::Errored || state_ == State::Stopping);
    return false;
  }

  output_ = frames;
  timestamp_offset_ = timestamp_offset;
  input_ = data;
  input_size_ = size;
  input_pos_ = 0;

  state_ = State::Parsing;
  while (state_ == State::Parsing) {
    signal_.SignalAll();
    signal_.ResetAndWaitWhileUnlocked(lock);
  }

  output_ = nullptr;
  input_ = nullptr;
  input_size_ = 0;
  return state_ == State::Waiting;
}

int FFmpegDemuxer::OnRead(void* user, uint8_t* buffer, int size) {
  auto* that = reinterpret_cast<FFmpegDemuxer*>(user);
  std::unique_lock<Mutex> lock(that->mutex_);
  while (that->input_pos_ >= that->input_size_ &&
         (that->state_ == State::Parsing || that->state_ == State::Waiting)) {
    that->state_ = State::Waiting;
    that->signal_.SignalAll();
    that->signal_.ResetAndWaitWhileUnlocked(lock);
  }
  if (that->state_ != State::Parsing) {
    DCHECK(that->state_ == State::Errored || that->state_ == State::Stopping);
    return AVERROR_EOF;
  }

  DCHECK_LT(that->input_pos_, that->input_size_);
  size_t to_read = std::min<size_t>(size, that->input_size_ - that->input_pos_);
  memcpy(buffer, that->input_ + that->input_pos_, to_read);
  that->input_pos_ += to_read;
  return to_read;
}

void FFmpegDemuxer::ThreadMain() {
  // Allocate a context for custom IO.
  // NOTE: The buffer may be reallocated/resized by libavformat later.
  // It is always our responsibility to free it later with av_free.
  io_ = avio_alloc_context(
      reinterpret_cast<unsigned char*>(av_malloc(kInitialBufferSize)),
      kInitialBufferSize,
      0,     // write_flag (read-only)
      this,  // opaque user data
      &OnRead,
      nullptr,   // write callback (read-only)
      nullptr);  // seek callback (linear reads only)
  if (!io_)
    return OnError();
  if (!ReinitDemuxer())
    return OnError();

  // At this point, the demuxer has been created and initialized, which is only
  // after we have parsed the init segment.
  if (client_) {
    if (demuxer_ctx_->duration == 0 ||
        demuxer_ctx_->duration == AV_NOPTS_VALUE) {
      client_->OnLoadedMetaData(HUGE_VAL);
    } else {
      client_->OnLoadedMetaData(static_cast<double>(demuxer_ctx_->duration) /
                                AV_TIME_BASE);
    }
  }

  std::unique_lock<Mutex> lock(mutex_);
  while (true) {
    while (state_ == State::Waiting) {
      signal_.SignalAll();
      signal_.ResetAndWaitWhileUnlocked(lock);
    }
    if (state_ != State::Parsing)
      return;

    AVPacket pkt;
    {
      util::Unlocker<Mutex> unlock(&lock);
      int ret = av_read_frame(demuxer_ctx_.get(), &pkt);
      if (ret == AVERROR_SHAKA_RESET_DEMUXER) {
        // Special case for Shaka where we need to reinit the demuxer.
        VLOG(1) << "Reinitializing demuxer";
        {
          std::unique_lock<Mutex> lock(mutex_);
          // Re-read the input data with the new demuxer.
          input_pos_ = 0;
        }

        if (!ReinitDemuxer())
          return OnError();
        ret = av_read_frame(demuxer_ctx_.get(), &pkt);
      }
      if (ret < 0) {
        av_packet_unref(&pkt);
        LogError(ret);
        return OnError();
      }

      UpdateEncryptionInfo();

      // Ignore discard flags.  The demuxer will set this when we try to read
      // content behind media we have already read.
      pkt.flags &= ~AV_PKT_FLAG_DISCARD;

      VLOG(3) << "Read frame at dts=" << pkt.dts;
      DCHECK_EQ(pkt.stream_index, 0);
      DCHECK_EQ(demuxer_ctx_->nb_streams, 1u);
    }

    auto* frame = ffmpeg::FFmpegEncodedFrame::MakeFrame(&pkt, cur_stream_info_,
                                                        timestamp_offset_);
    if (frame) {
      // No need to unref |pkt| since it was moved into the encoded frame.
      DCHECK(output_);
      output_->emplace_back(frame);
    } else {
      av_packet_unref(&pkt);
      state_ = State::Errored;
      signal_.SignalAll();
      return;
    }
  }
}

bool FFmpegDemuxer::ReinitDemuxer() {
  demuxer_ctx_.reset();
  avio_flush(io_);

  AVFormatContext* demuxer = avformat_alloc_context();
  if (!demuxer)
    return false;
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
    LogError(open_code);
    return false;
  }

  demuxer_ctx_.reset(demuxer);
  const int find_code = avformat_find_stream_info(demuxer, nullptr);
  if (find_code < 0) {
    LogError(find_code);
    return false;
  }

  if (demuxer_ctx_->nb_streams == 0) {
    LOG(ERROR) << "FFmpeg was unable to find any streams";
    return false;
  }
  if (demuxer_ctx_->nb_streams > 1) {
    LOG(ERROR) << "Multiple streams in input not supported";
    return false;
  }

  AVStream* stream = demuxer_ctx_->streams[0];
  AVCodecParameters* params = stream->codecpar;
  const std::string expected_codec = GetCodec(mime_type_, params->codec_id);

  const char* actual_codec = avcodec_get_name(params->codec_id);
  if (NormalizeCodec(expected_codec) != actual_codec) {
    LOG(ERROR) << "Mismatch between codec string and media.  Codec string: '"
               << expected_codec << "', media codec: '" << actual_codec
               << "' (0x" << std::hex << params->codec_id << ")";
    return false;
  }

  cur_stream_info_.reset(new StreamInfo(
      mime_type_, expected_codec, params->codec_type == AVMEDIA_TYPE_VIDEO,
      {stream->time_base.num, stream->time_base.den},
      std::vector<uint8_t>{params->extradata,
                           params->extradata + params->extradata_size},
      params->width, params->height, params->channels, params->sample_rate));
  return true;
}

void FFmpegDemuxer::UpdateEncryptionInfo() {
  if (!client_)
    return;

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
          client_->OnEncrypted(eme::MediaKeyInitDataType::WebM,
                               cur_info->key_ids[i], cur_info->key_id_size);
        }
      }
    }
    if (!pssh.empty()) {
      client_->OnEncrypted(eme::MediaKeyInitDataType::Cenc, pssh.data(),
                           pssh.size());
    }
    av_encryption_init_info_free(info);

    av_stream_remove_side_data(demuxer_ctx_->streams[0],
                               AV_PKT_DATA_ENCRYPTION_INIT_INFO);
  }
}

void FFmpegDemuxer::OnError() {
  std::unique_lock<Mutex> lock(mutex_);
  if (state_ != State::Stopping)
    state_ = State::Errored;
  signal_.SignalAllIfNotSet();
}


bool FFmpegDemuxerFactory::IsTypeSupported(const std::string& mime_type) const {
  std::string unused;
  return ParseAndCheckSupport(mime_type, &unused);
}

bool FFmpegDemuxerFactory::IsCodecVideo(const std::string& codec) const {
  std::string norm = NormalizeCodec(codec);
  auto* impl = avcodec_find_decoder_by_name(norm.c_str());
  return impl && impl->type == AVMEDIA_TYPE_VIDEO;
}

std::unique_ptr<Demuxer> FFmpegDemuxerFactory::Create(
    const std::string& mime_type, Demuxer::Client* client) const {
  std::string container;
  if (!ParseAndCheckSupport(mime_type, &container))
    return nullptr;

  return std::unique_ptr<Demuxer>(
      new (std::nothrow) FFmpegDemuxer(client, mime_type, container));
}

}  // namespace ffmpeg
}  // namespace media
}  // namespace shaka
