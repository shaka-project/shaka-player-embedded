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

#include "shaka/media/default_media_player.h"

namespace shaka {
namespace media {

class DefaultMediaPlayer::Impl {};

DefaultMediaPlayer::DefaultMediaPlayer(VideoRendererNew* video_renderer,
                                       AudioRendererNew* audio_renderer) {}
DefaultMediaPlayer::~DefaultMediaPlayer() {}

void DefaultMediaPlayer::SetDecoders(Decoder* video_decoder,
                                     Decoder* audio_decoder) {}

MediaCapabilitiesInfo DefaultMediaPlayer::DecodingInfo(
    const MediaDecodingConfiguration& config) const {
  return MediaCapabilitiesInfo();
}

MediaPlayer* DefaultMediaPlayer::CreateMse() {
  return nullptr;
}
MediaPlayer* DefaultMediaPlayer::CreateSource(const std::string& src) {
  return nullptr;
}

}  // namespace media
}  // namespace shaka
