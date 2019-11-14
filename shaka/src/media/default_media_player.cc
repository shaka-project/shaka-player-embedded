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

#include "src/media/mse_media_player.h"

namespace shaka {
namespace media {

class DefaultMediaPlayer::Impl {
 public:
  Impl(VideoRenderer* video_renderer, AudioRenderer* audio_renderer)
      : mse_player(video_renderer, audio_renderer) {}

  MseMediaPlayer mse_player;
};

DefaultMediaPlayer::DefaultMediaPlayer(VideoRenderer* video_renderer,
                                       AudioRenderer* audio_renderer)
    : impl_(new Impl(video_renderer, audio_renderer)) {}
DefaultMediaPlayer::~DefaultMediaPlayer() {}

void DefaultMediaPlayer::SetDecoders(Decoder* video_decoder,
                                     Decoder* audio_decoder) {
  impl_->mse_player.SetDecoders(video_decoder, audio_decoder);
}

MediaCapabilitiesInfo DefaultMediaPlayer::DecodingInfo(
    const MediaDecodingConfiguration& config) const {
  if (config.type == MediaDecodingType::File)
    return MediaCapabilitiesInfo();
  else
    return impl_->mse_player.DecodingInfo(config);
}

MediaPlayer* DefaultMediaPlayer::CreateMse() {
  if (!impl_->mse_player.AttachMse())
    return nullptr;
  return &impl_->mse_player;
}
MediaPlayer* DefaultMediaPlayer::CreateSource(const std::string& src) {
  return nullptr;
}

}  // namespace media
}  // namespace shaka
