// Copyright 2020 Google LLC
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

#include "shaka/eme/configuration.h"

namespace shaka {
namespace eme {

EncryptionPattern::EncryptionPattern() : encrypted_blocks(0), clear_blocks(0) {}

EncryptionPattern::EncryptionPattern(uint32_t encrypted_blocks,
                                     uint32_t clear_blocks)
    : encrypted_blocks(encrypted_blocks), clear_blocks(clear_blocks) {}


SubsampleInfo::SubsampleInfo(uint32_t clear_bytes, uint32_t protected_bytes)
    : clear_bytes(clear_bytes), protected_bytes(protected_bytes) {}


FrameEncryptionInfo::FrameEncryptionInfo(EncryptionScheme scheme,
                                         const std::vector<uint8_t>& key_id,
                                         const std::vector<uint8_t>& iv)
    : scheme(scheme), key_id(key_id), iv(iv) {}

FrameEncryptionInfo::FrameEncryptionInfo(EncryptionScheme scheme,
                                         EncryptionPattern pattern,
                                         const std::vector<uint8_t>& key_id,
                                         const std::vector<uint8_t>& iv)
    : scheme(scheme), pattern(pattern), key_id(key_id), iv(iv) {}

FrameEncryptionInfo::FrameEncryptionInfo(
    EncryptionScheme scheme, EncryptionPattern pattern,
    const std::vector<uint8_t>& key_id, const std::vector<uint8_t>& iv,
    const std::vector<SubsampleInfo>& subsamples)
    : scheme(scheme),
      pattern(pattern),
      key_id(key_id),
      iv(iv),
      subsamples(subsamples) {}

FrameEncryptionInfo::~FrameEncryptionInfo() {}

}  // namespace eme
}  // namespace shaka
