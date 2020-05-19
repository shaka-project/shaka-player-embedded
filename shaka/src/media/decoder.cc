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

#include "shaka/media/decoder.h"

#if defined(HAS_FFMPEG_DECODER)
#  include "src/media/ffmpeg/ffmpeg_decoder.h"
#elif defined(HAS_APPLE_DECODER)
#  include "src/media/apple/apple_decoder.h"
#endif

namespace shaka {
namespace media {

// \cond Doxygen_Skip
Decoder::Decoder() {}
Decoder::~Decoder() {}
// \endcond Doxygen_Skip

std::unique_ptr<Decoder> Decoder::CreateDefaultDecoder() {
#if defined(HAS_FFMPEG_DECODER)
  return std::unique_ptr<Decoder>(new ffmpeg::FFmpegDecoder);
#elif defined(HAS_APPLE_DECODER)
  return std::unique_ptr<Decoder>(new apple::AppleDecoder);
#else
  return nullptr;
#endif
}

}  // namespace media
}  // namespace shaka
