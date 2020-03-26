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

#include "src/media/ffmpeg/ffmpeg_encoded_frame.h"

extern "C" {
#include <libavutil/encryption_info.h>
}

#include <glog/logging.h>

#include <limits>
#include <memory>
#include <vector>

#include "shaka/eme/configuration.h"
#include "shaka/eme/implementation.h"

namespace shaka {
namespace media {
namespace ffmpeg {

namespace {

constexpr const uint32_t kCencScheme = 0x63656e63;
constexpr const uint32_t kCbcsScheme = 0x63626373;

bool MakeEncryptionInfo(const AVPacket* packet,
                        std::shared_ptr<eme::FrameEncryptionInfo>* info) {
  int side_data_size;
  uint8_t* side_data = av_packet_get_side_data(
      packet, AV_PKT_DATA_ENCRYPTION_INFO, &side_data_size);
  if (!side_data) {
    return true;
  }

  std::unique_ptr<AVEncryptionInfo, void (*)(AVEncryptionInfo*)> enc_info(
      av_encryption_info_get_side_data(side_data, side_data_size),
      &av_encryption_info_free);
  if (!enc_info) {
    LOG(DFATAL) << "Could not allocate new encryption info structure.";
    return false;
  }

  std::vector<eme::SubsampleInfo> subsamples;
  for (uint32_t i = 0; i < enc_info->subsample_count; i++) {
    subsamples.emplace_back(enc_info->subsamples[i].bytes_of_clear_data,
                            enc_info->subsamples[i].bytes_of_protected_data);
  }

  eme::EncryptionScheme scheme;
  eme::EncryptionPattern pattern{0, 0};
  if (enc_info->scheme == kCencScheme) {
    scheme = eme::EncryptionScheme::AesCtr;
  } else if (enc_info->scheme == kCbcsScheme) {
    scheme = eme::EncryptionScheme::AesCbc;
    pattern = eme::EncryptionPattern{enc_info->crypt_byte_block,
                                     enc_info->skip_byte_block};
  } else {
    LOG(DFATAL) << "Unsupported scheme";
    return false;
  }

  auto make_vec = [](const uint8_t* data, size_t size) {
    return std::vector<uint8_t>(data, data + size);
  };
  *info = std::make_shared<eme::FrameEncryptionInfo>(
      scheme, pattern, make_vec(enc_info->key_id, enc_info->key_id_size),
      make_vec(enc_info->iv, enc_info->iv_size), subsamples);
  return true;
}

bool IsEncrypted(const AVPacket* packet) {
  return av_packet_get_side_data(packet, AV_PKT_DATA_ENCRYPTION_INFO, nullptr);
}

}  // namespace

// static
FFmpegEncodedFrame* FFmpegEncodedFrame::MakeFrame(
    AVPacket* pkt, std::shared_ptr<const StreamInfo> info,
    double timestamp_offset) {
  const double factor = info->time_scale;
  const double pts = pkt->pts * factor + timestamp_offset;
  const double dts = pkt->dts * factor + timestamp_offset;
  const double duration = pkt->duration * factor;
  const bool is_key_frame = pkt->flags & AV_PKT_FLAG_KEY;

  return new (std::nothrow) FFmpegEncodedFrame(
      pkt, pts, dts, duration, is_key_frame, info, timestamp_offset);
}

FFmpegEncodedFrame::~FFmpegEncodedFrame() {
  av_packet_unref(&packet_);
}

MediaStatus FFmpegEncodedFrame::Decrypt(const eme::Implementation* cdm,
                                        uint8_t* dest) const {
  if (!is_encrypted)
    return MediaStatus::Success;
  if (!cdm)
    return MediaStatus::FatalError;

  std::shared_ptr<eme::FrameEncryptionInfo> info;
  if (!MakeEncryptionInfo(&packet_, &info)) {
    LOG(DFATAL) << "Unable to create encryption info.";
    return MediaStatus::FatalError;
  }
  const eme::DecryptStatus decrypt_status =
      cdm->Decrypt(info.get(), packet_.data, packet_.size, dest);
  switch (decrypt_status) {
    case eme::DecryptStatus::Success:
      return MediaStatus::Success;
    case eme::DecryptStatus::KeyNotFound:
      return MediaStatus::KeyNotFound;
    default:
      return MediaStatus::FatalError;
  }
}

size_t FFmpegEncodedFrame::EstimateSize() const {
  size_t size = sizeof(*this) + packet_.size;
  for (int i = packet_.side_data_elems; i; i--)
    size += packet_.side_data[i - 1].size;
  return size;
}

FFmpegEncodedFrame::FFmpegEncodedFrame(AVPacket* pkt, double pts, double dts,
                                       double duration, bool is_key_frame,
                                       std::shared_ptr<const StreamInfo> info,
                                       double timestamp_offset)
    : EncodedFrame(info, pts, dts, duration, is_key_frame, pkt->data, pkt->size,
                   timestamp_offset, IsEncrypted(pkt)) {
  av_packet_move_ref(&packet_, pkt);
}

}  // namespace ffmpeg
}  // namespace media
}  // namespace shaka
