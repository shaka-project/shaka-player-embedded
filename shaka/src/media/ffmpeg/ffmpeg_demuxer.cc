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
#include "src/util/buffer_reader.h"
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

// The FFmpeg demuxer will use its decoders to fill in certain fields.  If we
// aren't using the FFmpeg decoders, we need to parse these fields ourselves.
#ifndef HAS_FFMPEG_DECODER

void RemoveEmulationPrevention(const uint8_t* data, size_t size,
                               std::vector<uint8_t>* output) {
  DCHECK_EQ(output->size(), size);
  // A byte sequence 0x0 0x0 0x1 is used to signal the start of a NALU.  So for
  // the body of the NALU, it needs to be escaped.  So this reverses the
  // escaping by changing 0x0 0x0 0x3 to 0x0 0x0.
  DCHECK_EQ(output->size(), size);
  size_t out_pos = 0;
  for (size_t in_pos = 0; in_pos < size;) {
    if (in_pos + 2 < size && data[in_pos] == 0 && data[in_pos + 1] == 0 &&
        data[in_pos + 2] == 0x3) {
      (*output)[out_pos++] = 0;
      (*output)[out_pos++] = 0;
      in_pos += 3;
    } else {
      (*output)[out_pos++] = data[in_pos++];
    }
  }
  output->resize(out_pos);
}

Rational<uint32_t> GetSarFromVuiParameters(util::BufferReader* reader) {
  // See section E.1.1 of H.264/H.265.
  // vui_parameters()
  if (reader->ReadBits(1) == 0)  // aspect_ratio_info_present_flag
    return {0, 0};               // Values we want aren't there, return unknown.
  const uint8_t aspect_ratio_idc = reader->ReadUint8();
  // See Table E-1 in H.264.
  switch (aspect_ratio_idc) {
    case 1:
      return {1, 1};
    case 2:
      return {12, 11};
    case 3:
      return {10, 11};
    case 4:
      return {16, 11};
    case 5:
      return {40, 33};
    case 6:
      return {24, 11};
    case 7:
      return {20, 11};
    case 8:
      return {32, 11};
    case 9:
      return {80, 33};
    case 10:
      return {18, 11};
    case 11:
      return {15, 11};
    case 12:
      return {64, 33};
    case 13:
      return {160, 99};
    case 14:
      return {4, 3};
    case 15:
      return {3, 2};
    case 16:
      return {2, 1};
    case 255:
      return {reader->ReadBits(16), reader->ReadBits(16)};

    default:
      LOG(DFATAL) << "Unknown value of aspect_ratio_idc: "
                  << static_cast<int>(aspect_ratio_idc);
      return {0, 0};
  }
}

Rational<uint32_t> GetSarFromH264(const std::vector<uint8_t>& extra_data) {
  util::BufferReader reader(extra_data.data(), extra_data.size());
  // The H.264 extra data is a AVCDecoderConfigurationRecord from
  // Section 5.3.3.1.2 in ISO/IEC 14496-15
  reader.Skip(5);
  const size_t sps_count = reader.ReadUint8() & 0x1f;
  if (sps_count == 0)
    return {0, 0};

  // There should only be one SPS, or they should be compatible since there
  // should only be one video stream.  There may be two SPS for encrypted
  // content with a clear lead.
  const size_t sps_size = reader.ReadBits(16);
  if (sps_size >= reader.BytesRemaining()) {
    LOG(DFATAL) << "Invalid avcC configuration";
    return {0, 0};
  }

  // This is an SPS NALU; remove the emulation prevention bytes.
  // See ISO/IE 14496-10 Sec. 7.3.1/7.3.2 and H.264 Sec. 7.3.2.1.1.
  std::vector<uint8_t> temp(sps_size);
  RemoveEmulationPrevention(reader.data(), sps_size, &temp);
  util::BufferReader sps_reader(temp.data(), temp.size());
  if (sps_reader.ReadUint8() != 0x67) {
    LOG(DFATAL) << "Non-SPS found in avcC configuration";
    return {0, 0};
  }

  // seq_parameter_set_rbsp()
  const uint8_t profile_idc = sps_reader.ReadUint8();
  sps_reader.Skip(2);
  sps_reader.ReadExpGolomb();  // seq_parameter_set_id
  // Values here copied from the H.264 spec.
  if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
      profile_idc == 244 || profile_idc == 44 || profile_idc == 83 ||
      profile_idc == 86 || profile_idc == 118 || profile_idc == 128 ||
      profile_idc == 138 || profile_idc == 139 || profile_idc == 134) {
    const uint64_t chroma_format_idc = sps_reader.ReadExpGolomb();
    if (chroma_format_idc == 3)
      sps_reader.ReadBits(1);           // separate_colour_plane_flag
    sps_reader.ReadExpGolomb();         // bit_depth_luma_minus8
    sps_reader.ReadExpGolomb();         // bit_depth_chroma_minus8
    sps_reader.SkipBits(1);             // qpprime_y_zero_transform_bypass_flag
    if (sps_reader.ReadBits(1) == 1) {  // seq_scaling_matrix_present_flag
      LOG(WARNING) << "Scaling matrix is unsupported";
      return {0, 0};
    }
  }
  sps_reader.ReadExpGolomb();  // log2_max_frame_num_minus4
  const uint64_t pic_order_cnt_type = sps_reader.ReadExpGolomb();
  if (pic_order_cnt_type == 0) {
    sps_reader.ReadExpGolomb();  // log2_max_pic_order_cnt_lsb_minus4
  } else if (pic_order_cnt_type == 1) {
    sps_reader.ReadBits(1);      // delta_pic_order_always_zero_flag
    sps_reader.ReadExpGolomb();  // offset_for_non_ref_pic
    sps_reader.ReadExpGolomb();  // offset_for_top_to_bottom_field
    const uint64_t count = sps_reader.ReadExpGolomb();
    for (uint64_t i = 0; i < count; i++)
      sps_reader.ReadExpGolomb();  // offset_for_ref_frame
  }
  sps_reader.ReadExpGolomb();         // max_num_ref_frames
  sps_reader.ReadBits(1);             // gaps_in_frame_num_value_allowed_flag
  sps_reader.ReadExpGolomb();         // pic_width_in_mbs_minus1
  sps_reader.ReadExpGolomb();         // pic_height_in_map_units_minus1
  if (sps_reader.ReadBits(1) == 0)    // frame_mbs_only_flag
    sps_reader.ReadBits(1);           // mb_adaptive_frame_field_flag
  sps_reader.ReadBits(1);             // direct_8x8_inference_flag
  if (sps_reader.ReadBits(1) == 1) {  // frame_cropping_flag
    sps_reader.ReadExpGolomb();       // pframe_crop_left_offset
    sps_reader.ReadExpGolomb();       // pframe_crop_right_offset
    sps_reader.ReadExpGolomb();       // pframe_crop_top_offset
    sps_reader.ReadExpGolomb();       // pframe_crop_bottom_offset
  }
  if (sps_reader.ReadBits(1) == 0)  // vui_parameters_present_flag
    return {0, 0};  // Values we want aren't there, return unknown.
  // Finally, the thing we actually care about, display parameters.
  return GetSarFromVuiParameters(&sps_reader);
}

void SkipHevcProfileTierLevel(bool profile_present,
                              uint64_t max_sub_layers_minus1,
                              util::BufferReader* reader) {
  if (profile_present) {
    reader->Skip(11);
  }
  reader->Skip(1);
  std::vector<bool> sub_layer_profile_present_flag(max_sub_layers_minus1);
  std::vector<bool> sub_layer_level_present_flag(max_sub_layers_minus1);
  for (uint64_t i = 0; i < max_sub_layers_minus1; i++) {
    sub_layer_profile_present_flag[i] = reader->ReadBits(1);
    sub_layer_level_present_flag[i] = reader->ReadBits(1);
  }
  if (max_sub_layers_minus1 > 0 && max_sub_layers_minus1 < 8)
    reader->SkipBits(2 * (8 - max_sub_layers_minus1));
  for (uint64_t i = 0; i < max_sub_layers_minus1; i++) {
    if (sub_layer_profile_present_flag[i])
      reader->Skip(11);
    if (sub_layer_level_present_flag[i])
      reader->Skip(1);
  }
}

Rational<uint32_t> GetSarFromHevc(const std::vector<uint8_t>& extra_data) {
  util::BufferReader reader(extra_data.data(), extra_data.size());
  // The H.265 extra data is a HEVCDecoderConfigurationRecord from
  // Section 8.3.3.1.2 in ISO/IEC 14496-15
  reader.Skip(22);
  const uint8_t num_of_arrays = reader.ReadUint8();
  uint64_t nalu_length = 0;
  bool found = false;
  for (uint8_t i = 0; i < num_of_arrays && !found; i++) {
    const uint8_t nalu_type = reader.ReadUint8() & 0x3f;
    const uint64_t num_nalus = reader.ReadBits(16);
    for (uint64_t i = 0; i < num_nalus; i++) {
      nalu_length = reader.ReadBits(16);
      // Find the first SPS NALU.  Since this stream should only have one video
      // stream, all SPS should be compatible.
      if (nalu_type == 33) {
        found = true;
        break;
      }
      reader.Skip(nalu_length);
    }
  }
  if (!found)
    return {0, 0};  // No SPS found, return unknown.

  // This is an SPS NALU; remove the emulation prevention bytes.
  // See H.265 Sec. 7.3.1.2/7.3.2.2.1.
  std::vector<uint8_t> temp(nalu_length);
  RemoveEmulationPrevention(reader.data(), nalu_length, &temp);
  util::BufferReader sps_reader(temp.data(), temp.size());
  const uint64_t nalu_type = (sps_reader.ReadBits(16) >> 9) & 0x3f;
  if (nalu_type != 33) {
    LOG(DFATAL) << "Invalid NALU type found in extra data";
    return {0, 0};
  }

  sps_reader.SkipBits(4);  // sps_video_parameter_set_id
  const uint64_t max_sub_layers_minus1 = sps_reader.ReadBits(3);
  sps_reader.SkipBits(1);  // sps_temporal_id_nesting_flag
  SkipHevcProfileTierLevel(/* profile_present= */ true, max_sub_layers_minus1,
                           &sps_reader);
  sps_reader.ReadExpGolomb();           // sps_seq_parameter_set_id
  if (sps_reader.ReadExpGolomb() == 3)  // chroma_format_idc
    sps_reader.SkipBits(1);             // separate_colour_plane_flag
  sps_reader.ReadExpGolomb();           // pic_width_in_luma_samples
  sps_reader.ReadExpGolomb();           // pic_height_in_luma_samples
  if (sps_reader.ReadBits(1) == 1) {    // conformance_window_flag
    sps_reader.ReadExpGolomb();         // conf_win_left_offset
    sps_reader.ReadExpGolomb();         // conf_win_right_offset
    sps_reader.ReadExpGolomb();         // conf_win_top_offset
    sps_reader.ReadExpGolomb();         // conf_win_bottom_offset
  }
  sps_reader.ReadExpGolomb();  // bit_depth_luma_minus8
  sps_reader.ReadExpGolomb();  // bit_depth_chroma_minus8
  sps_reader.ReadExpGolomb();  // log2_max_pic_order_cnt_lsb_minus4
  const uint64_t sub_layer_ordering_info_present = sps_reader.ReadBits(1);
  for (uint64_t i =
           (sub_layer_ordering_info_present ? 0 : max_sub_layers_minus1);
       i <= max_sub_layers_minus1; i++) {
    sps_reader.ReadExpGolomb();  // sps_max_dec_pic_buffering_minus1
    sps_reader.ReadExpGolomb();  // sps_max_num_reorder_pics
    sps_reader.ReadExpGolomb();  // ps_max_latency_increase_plus1
  }
  sps_reader.ReadExpGolomb();  // log2_min_luma_coding_block_size_minus3
  sps_reader.ReadExpGolomb();  // log2_diff_max_min_luma_coding_block_size
  sps_reader.ReadExpGolomb();  // log2_min_luma_transform_block_size_minus2
  sps_reader.ReadExpGolomb();  // log2_diff_max_min_luma_transform_block_size
  sps_reader.ReadExpGolomb();  // max_transform_hierarchy_depth_inter
  sps_reader.ReadExpGolomb();  // max_transform_hierarchy_depth_intra
  if (sps_reader.ReadBits(1) == 1) {  // scaling_list_enabled_flag
    LOG(WARNING) << "Scaling list isn't supported";
    return {0, 0};
  }
  sps_reader.SkipBits(1);             // amp_enabled_flag
  sps_reader.SkipBits(1);             // sample_adaptive_offset_enabled_flag
  if (sps_reader.ReadBits(1) == 1) {  // pcm_enabled_flag
    sps_reader.ReadBits(4);           // pcm_sample_bit_depth_luma_minus1
    sps_reader.ReadBits(4);           // pcm_sample_bit_depth_chroma_minus1
    sps_reader.ReadExpGolomb();  // log2_min_pcm_luma_coding_block_size_minus3
    sps_reader.ReadExpGolomb();  // log2_diff_max_min_pcm_luma_coding_block_size
  }
  const uint64_t num_short_term_ref_pic_sets = sps_reader.ReadExpGolomb();
  if (num_short_term_ref_pic_sets != 0) {
    LOG(WARNING) << "Short-term reference pictures not supported";
    return {0, 0};
  }
  if (sps_reader.ReadBits(1) == 1) {  // long_term_ref_pics_present_flag
    const uint64_t num_long_term_ref_pics_sps = sps_reader.ReadExpGolomb();
    for (uint64_t i = 0; i < num_long_term_ref_pics_sps; i++) {
      sps_reader.ReadExpGolomb();  // lt_ref_pic_poc_lsb_sps
      sps_reader.ReadBits(1);      // used_by_curr_pic_lt_sps_flag
    }
  }
  sps_reader.ReadBits(1);           // sps_temporal_mvp_enabled_flag
  sps_reader.ReadBits(1);           // strong_intra_smoothing_enabled_flag
  if (sps_reader.ReadBits(1) != 1)  // vui_parameters_present_flag
    return {0, 0};  // The info we want isn't there, return unknown.
  return GetSarFromVuiParameters(&sps_reader);
}

#endif

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

  std::vector<uint8_t> extra_data{params->extradata,
                                  params->extradata + params->extradata_size};
  Rational<uint32_t> sar{params->sample_aspect_ratio.num,
                         params->sample_aspect_ratio.den};
#ifndef HAS_FFMPEG_DECODER
  if (!sar) {
    switch (params->codec_id) {
      case AV_CODEC_ID_H264:
        sar = GetSarFromH264(extra_data);
        break;
      case AV_CODEC_ID_H265:
        sar = GetSarFromHevc(extra_data);
        break;
      default:
        break;
    }
  }
#endif

  cur_stream_info_.reset(new StreamInfo(
      mime_type_, expected_codec, params->codec_type == AVMEDIA_TYPE_VIDEO,
      {stream->time_base.num, stream->time_base.den}, sar, extra_data,
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
  if (impl)
    return impl->type == AVMEDIA_TYPE_VIDEO;
  return norm == "h264" || norm == "hevc" || norm == "vp8" || norm == "vp9";
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
