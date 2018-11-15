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

#include "src/util/crypto.h"

#include <CommonCrypto/CommonDigest.h>

#include <glog/logging.h>

namespace shaka {
namespace util {

std::vector<uint8_t> HashData(const uint8_t* data, size_t size) {
  std::vector<uint8_t> ret(CC_MD5_DIGEST_LENGTH);
  CHECK(CC_MD5(data, size, ret.data()));
  return ret;
}

}  // namespace util
}  // namespace shaka
