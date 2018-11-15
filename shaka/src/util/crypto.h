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

#ifndef SHAKA_EMBEDDED_UTIL_CRYPTO_H_
#define SHAKA_EMBEDDED_UTIL_CRYPTO_H_

#include <stddef.h>  // for size_t
#include <stdint.h>  // for uint8_t

#include <vector>

namespace shaka {
namespace util {

/** Calculates the MD5 hash of the given data. */
std::vector<uint8_t> HashData(const uint8_t* data, size_t size);

}  // namespace util
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTIL_CRYPTO_H_
