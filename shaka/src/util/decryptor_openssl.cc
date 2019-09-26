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

#include <glog/logging.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#include "src/util/decryptor.h"

namespace shaka {
namespace util {

struct Decryptor::Impl {
  std::unique_ptr<EVP_CIPHER_CTX, void (*)(EVP_CIPHER_CTX*)> ctx{
      nullptr, &EVP_CIPHER_CTX_free};
};

Decryptor::Decryptor(eme::EncryptionScheme scheme,
                     const std::vector<uint8_t>& key,
                     const std::vector<uint8_t>& iv)
    : scheme_(scheme), key_(key), iv_(iv), extra_(new Impl) {
  DCHECK_EQ(AES_BLOCK_SIZE, key.size());
  DCHECK_EQ(AES_BLOCK_SIZE, iv.size());
}

Decryptor::~Decryptor() {}

bool Decryptor::DecryptPartialBlock(const uint8_t* data, size_t data_size,
                                    uint32_t block_offset, uint8_t* dest) {
  DCHECK_LE(block_offset + data_size, AES_BLOCK_SIZE);

  if (!InitIfNeeded())
    return false;

  if (scheme_ != eme::EncryptionScheme::AesCtr) {
    LOG(ERROR) << "Cannot have block offset when using CBC";
    return false;
  }

  uint8_t temp_source[AES_BLOCK_SIZE] = {0};
  uint8_t temp_dest[AES_BLOCK_SIZE];
  const size_t partial_size =
      std::min<size_t>(AES_BLOCK_SIZE - block_offset, data_size);
  memcpy(temp_source + block_offset, data, partial_size);

  int num_bytes_read;
  if (!EVP_DecryptUpdate(extra_->ctx.get(), temp_dest, &num_bytes_read,
                         temp_source, AES_BLOCK_SIZE) ||
      num_bytes_read != AES_BLOCK_SIZE) {
    LOG(ERROR) << "Error decrypting data: "
               << ERR_error_string(ERR_get_error(), nullptr);
    return false;
  }
  memcpy(dest, temp_dest + block_offset, partial_size);
  return true;
}

bool Decryptor::Decrypt(const uint8_t* data, size_t data_size, uint8_t* dest) {
  if (!InitIfNeeded())
    return false;

  int num_bytes_read;
  if (!EVP_DecryptUpdate(extra_->ctx.get(), dest, &num_bytes_read, data,
                         data_size) ||
      static_cast<size_t>(num_bytes_read) != data_size) {
    LOG(ERROR) << "Error decrypting data: "
               << ERR_error_string(ERR_get_error(), nullptr);
    return false;
  }

  return true;
}

bool Decryptor::InitIfNeeded() {
  if (!extra_->ctx) {
    extra_->ctx.reset(EVP_CIPHER_CTX_new());
    if (!extra_->ctx) {
      LOG(ERROR) << "Error allocating OpenSSL context: "
                 << ERR_error_string(ERR_get_error(), nullptr);
      return false;
    }

    const bool is_ctr = scheme_ == eme::EncryptionScheme::AesCtr;
    if (!EVP_DecryptInit_ex(extra_->ctx.get(),
                            is_ctr ? EVP_aes_128_ctr() : EVP_aes_128_cbc(),
                            nullptr, key_.data(), iv_.data()) ||
        !EVP_CIPHER_CTX_set_padding(extra_->ctx.get(), 0)) {
      LOG(ERROR) << "Error setting up OpenSSL context: "
                 << ERR_error_string(ERR_get_error(), nullptr);
      return false;
    }
  }
  return true;
}

}  // namespace util
}  // namespace shaka
