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

#ifndef SHAKA_EMBEDDED_UTIL_DECRYPTORS_H_
#define SHAKA_EMBEDDED_UTIL_DECRYPTORS_H_

#include <memory>
#include <vector>

#include "shaka/eme/configuration.h"
#include "src/util/macros.h"

#define AES_BLOCK_SIZE 16u

namespace shaka {
namespace util {

/**
 * A utility class that decrypts data.  This stores the current decryption state
 * so it can be reused for a single decrypt operation.  This will only succeed
 * if all the data is decrypted, meaning for CBC, a whole AES block needs to be
 * given.  It is assumed the output is at least the same size as the input.
 */
class Decryptor {
 public:
  Decryptor(eme::EncryptionScheme scheme, const std::vector<uint8_t>& key,
            const std::vector<uint8_t>& iv);
  ~Decryptor();

  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(Decryptor);

  /**
   * Decrypts the given partial block into the given buffer.  This must be
   * given a partial block and |data_size + block_offset <= AES_BLOCK_SIZE|.
   */
  bool DecryptPartialBlock(const uint8_t* data, size_t data_size,
                           uint32_t block_offset, uint8_t* dest);

  /**
   * Decrypts the given data into the given buffer.  This data size must be a
   * multiple of AES_BLOCK_SIZE.
   */
  bool Decrypt(const uint8_t* data, size_t data_size, uint8_t* dest);

 private:
  bool InitIfNeeded();

  const eme::EncryptionScheme scheme_;
  const std::vector<uint8_t> key_;
  std::vector<uint8_t> iv_;

  struct Impl;
  std::unique_ptr<Impl> extra_;
};

}  // namespace util
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTIL_DECRYPTORS_H_
