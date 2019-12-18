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
#include <netinet/in.h>

#include <limits>
#include <memory>
#include <vector>

#include "shaka/eme/configuration.h"
#include "shaka/eme/implementation.h"

namespace shaka {
namespace media {
namespace ffmpeg {

namespace {

constexpr const size_t kAesBlockSize = 16;
constexpr const uint32_t kCencScheme = 0x63656e63;
constexpr const uint32_t kCensScheme = 0x63656e73;
constexpr const uint32_t kCbc1Scheme = 0x63626331;
constexpr const uint32_t kCbcsScheme = 0x63626373;

void IncrementIv(uint32_t count, std::vector<uint8_t>* iv) {
  DCHECK_EQ(iv->size(), 16u);
  auto* counter = reinterpret_cast<uint32_t*>(&iv->at(8));
  // The IV is a big-endian integer.  ntohl will convert a big-endian number to
  // a host byte order; htonl does the opposite.  Since these are unsigned,
  // overflow is well-defined.
  constexpr const uint32_t max = std::numeric_limits<uint32_t>::max();
  if (max - count < ntohl(counter[1])) {
    counter[0] = htonl(ntohl(counter[0]) + 1);
  }
  counter[1] = htonl(ntohl(counter[1]) + count);
}

bool IsEncrypted(AVPacket* packet) {
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

  int side_data_size;
  uint8_t* side_data = av_packet_get_side_data(
      &packet_, AV_PKT_DATA_ENCRYPTION_INFO, &side_data_size);
  if (!side_data) {
    LOG(ERROR) << "Unable to get side data from packet.";
    return MediaStatus::FatalError;
  }

  std::unique_ptr<AVEncryptionInfo, void (*)(AVEncryptionInfo*)> enc_info(
      av_encryption_info_get_side_data(side_data, side_data_size),
      &av_encryption_info_free);
  if (!enc_info) {
    LOG(ERROR) << "Could not allocate new encryption info structure.";
    return MediaStatus::FatalError;
  }

  eme::EncryptionScheme scheme;
  switch (enc_info->scheme) {
    case kCencScheme:
      if (enc_info->crypt_byte_block != 0 || enc_info->skip_byte_block != 0) {
        LOG(ERROR) << "Cannot specify encryption pattern with 'cenc' scheme.";
        return MediaStatus::FatalError;
      }
      scheme = eme::EncryptionScheme::AesCtr;
      break;
    case kCensScheme:
      scheme = eme::EncryptionScheme::AesCtr;
      break;
    case kCbc1Scheme:
      if (enc_info->crypt_byte_block != 0 || enc_info->skip_byte_block != 0) {
        LOG(ERROR) << "Cannot specify encryption pattern with 'cbc1' scheme.";
        return MediaStatus::FatalError;
      }
      scheme = eme::EncryptionScheme::AesCbc;
      break;
    case kCbcsScheme:
      scheme = eme::EncryptionScheme::AesCbc;
      break;

    default:
      LOG(ERROR) << "Scheme 0x" << std::hex << enc_info->scheme
                 << " is unsupported";
      return MediaStatus::FatalError;
  }

  if (enc_info->subsample_count == 0) {
    const eme::DecryptStatus decrypt_status =
        cdm->Decrypt(scheme,
                     eme::EncryptionPattern{enc_info->crypt_byte_block,
                                            enc_info->skip_byte_block},
                     0, enc_info->key_id, enc_info->key_id_size, enc_info->iv,
                     enc_info->iv_size, packet_.data, packet_.size, dest);
    switch (decrypt_status) {
      case eme::DecryptStatus::Success:
        break;
      case eme::DecryptStatus::KeyNotFound:
        return MediaStatus::KeyNotFound;
      default:
        return MediaStatus::FatalError;
    }
  } else {
    const uint8_t* src = packet_.data;
    size_t total_size = packet_.size;
    size_t block_offset = 0;
    std::vector<uint8_t> cur_iv(enc_info->iv, enc_info->iv + enc_info->iv_size);
    for (uint32_t i = 0; i < enc_info->subsample_count; i++) {
      const unsigned int clear_bytes =
          enc_info->subsamples[i].bytes_of_clear_data;
      const unsigned int protected_bytes =
          enc_info->subsamples[i].bytes_of_protected_data;
      if (total_size < clear_bytes ||
          total_size - clear_bytes < protected_bytes) {
        LOG(ERROR) << "Invalid subsample size";
        return MediaStatus::FatalError;
      }

      // Copy clear content first.
      memcpy(dest, src, clear_bytes);
      src += clear_bytes;
      dest += clear_bytes;
      total_size -= clear_bytes;

      // If there is nothing to decrypt, skip to the next subsample.
      if (protected_bytes == 0)
        continue;

      // Decrypt encrypted content.
      const eme::DecryptStatus decrypt_status = cdm->Decrypt(
          scheme,
          eme::EncryptionPattern{enc_info->crypt_byte_block,
                                 enc_info->skip_byte_block},
          block_offset, enc_info->key_id, enc_info->key_id_size, cur_iv.data(),
          cur_iv.size(), src, protected_bytes, dest);
      switch (decrypt_status) {
        case eme::DecryptStatus::Success:
          break;
        case eme::DecryptStatus::KeyNotFound:
          return MediaStatus::KeyNotFound;
        default:
          return MediaStatus::FatalError;
      }

      if (enc_info->scheme == kCencScheme || enc_info->scheme == kCensScheme) {
        uint32_t increment = 0;
        if (enc_info->scheme == kCencScheme) {
          // Increment the IV for each AES block we decrypted; add the
          // block_offset to account for any partial block at the beginning.
          increment = (block_offset + protected_bytes) / kAesBlockSize;
        } else {
          // Increment the IV for each encrypted block within the pattern.
          const size_t num_blocks = protected_bytes / kAesBlockSize;
          const size_t pattern_size =
              enc_info->crypt_byte_block + enc_info->skip_byte_block;

          // Count the number of "pattern blocks" in the protected part of the
          // subsample.  If there is a partial pattern, include it only if it
          // has a whole crypt_byte_block.
          increment = (num_blocks / pattern_size) * enc_info->crypt_byte_block;
          if (num_blocks % pattern_size >= enc_info->crypt_byte_block)
            increment += enc_info->crypt_byte_block;
        }
        IncrementIv(increment, &cur_iv);
        block_offset = (block_offset + protected_bytes) % kAesBlockSize;
      } else if (enc_info->scheme == kCbc1Scheme) {
        // 'cbc1' uses cipher-block-chaining, so the IV should be the last
        // encrypted block of this subsample.
        if (protected_bytes < kAesBlockSize ||
            protected_bytes % kAesBlockSize != 0) {
          LOG(ERROR) << "'cbc1' requires subsamples to be a multiple of the "
                        "AES block size.";
          return MediaStatus::FatalError;
        }
        const uint8_t* block_end = src + protected_bytes;
        cur_iv.assign(block_end - kAesBlockSize, block_end);
      }
      // 'cbcs' uses constant-IV.

      src += protected_bytes;
      dest += protected_bytes;
      total_size -= protected_bytes;
    }

    if (total_size != 0) {
      LOG(ERROR) << "Extra remaining data after subsample handling";
      return MediaStatus::FatalError;
    }
  }

  return MediaStatus::Success;
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
