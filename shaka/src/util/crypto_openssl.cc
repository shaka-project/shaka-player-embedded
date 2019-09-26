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
#include <openssl/evp.h>

#include "src/util/crypto.h"

#define HASH_ALGORITHM EVP_md5()

namespace shaka {
namespace util {

std::vector<uint8_t> HashData(const uint8_t* data, size_t size) {
  EVP_MD_CTX* ctx = EVP_MD_CTX_create();
  CHECK(ctx);

  CHECK_EQ(EVP_DigestInit_ex(ctx, HASH_ALGORITHM, nullptr), 1u);
  CHECK_EQ(EVP_DigestUpdate(ctx, data, size), 1u);

  std::vector<uint8_t> result(EVP_MD_size(HASH_ALGORITHM));
  CHECK_EQ(EVP_DigestFinal_ex(ctx, result.data(), nullptr), 1u);
  return result;
}

}  // namespace util
}  // namespace shaka
