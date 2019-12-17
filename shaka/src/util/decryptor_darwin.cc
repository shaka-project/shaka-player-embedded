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

#include <CommonCrypto/CommonCryptor.h>
#include <Security/Security.h>
#include <glog/logging.h>  // NOLINT(build/include_alpha)

#include "src/util/decryptor.h"

namespace shaka {
namespace util {

namespace {

void IncrementIv(std::vector<uint8_t>* iv) {
  DCHECK_EQ(iv->size(), AES_BLOCK_SIZE);
  auto* iv_ptr = reinterpret_cast<uint64_t*>(iv->data() + 8);
  // This converts the number from network (big) byte order to host order, then
  // increases the value, then converts back to network byte order.
  *iv_ptr = htonll(ntohll(*iv_ptr) + 1);
}

}  // namespace

struct Decryptor::Impl {};

Decryptor::Decryptor(eme::EncryptionScheme scheme,
                     const std::vector<uint8_t>& key,
                     const std::vector<uint8_t>& iv)
    : scheme_(scheme), key_(key), iv_(iv) {
  DCHECK_EQ(AES_BLOCK_SIZE, key.size());
  DCHECK_EQ(AES_BLOCK_SIZE, iv.size());
}

Decryptor::~Decryptor() {}

bool Decryptor::DecryptPartialBlock(const uint8_t* data, size_t data_size,
                                    uint32_t block_offset, uint8_t* dest) {
  if (scheme_ == eme::EncryptionScheme::AesCtr) {
    // Mac/iOS only supports CBC, so we need to implement CTR mode based on
    // their AES encryption.

    size_t data_offset = 0;
    while (data_offset < data_size) {
      uint8_t encrypted_iv[AES_BLOCK_SIZE];
      size_t length;
      CCCryptorStatus result = CCCrypt(
          kCCEncrypt, kCCAlgorithmAES128, 0, key_.data(), key_.size(), nullptr,
          iv_.data(), iv_.size(), encrypted_iv, AES_BLOCK_SIZE, &length);
      if (result != kCCSuccess) {
        LOG(ERROR) << "Error decrypting data: " << result;
        return false;
      }
      if (length != AES_BLOCK_SIZE) {
        LOG(ERROR) << "Not all data decrypted";
        return false;
      }

      const size_t to_decrypt = AES_BLOCK_SIZE - block_offset;
      for (size_t i = 0; i < to_decrypt && i + data_offset < data_size; i++) {
        dest[data_offset + i] =
            data[data_offset + i] ^ encrypted_iv[i + block_offset];
      }

      data_offset += to_decrypt;
      block_offset = 0;
      IncrementIv(&iv_);
    }
  } else {
    if (block_offset != 0) {
      LOG(ERROR) << "Cannot have block offset when using CBC";
      return false;
    }
    if (data_size % AES_BLOCK_SIZE != 0u) {
      LOG(ERROR) << "CBC requires protected ranges to be a multiple of the "
                    "block size.";
      return false;
    }

    // This uses AES-CBC.
    size_t length;
    CCCryptorStatus result =
        CCCrypt(kCCDecrypt, kCCAlgorithmAES128, 0, key_.data(), key_.size(),
                iv_.data(), data, data_size, dest, data_size, &length);
    if (result != kCCSuccess) {
      LOG(ERROR) << "Error decrypting data: " << result;
      return false;
    }
    if (length != data_size) {
      LOG(ERROR) << "Not all data decrypted";
      return false;
    }

    if (data_size >= AES_BLOCK_SIZE) {
      iv_.assign(data + data_size - AES_BLOCK_SIZE, data + data_size);
    }
  }

  return true;
}

bool Decryptor::Decrypt(const uint8_t* data, size_t data_size, uint8_t* dest) {
  return DecryptPartialBlock(data, data_size, 0, dest);
}

bool Decryptor::InitIfNeeded() {
  return true;
}

}  // namespace util
}  // namespace shaka
