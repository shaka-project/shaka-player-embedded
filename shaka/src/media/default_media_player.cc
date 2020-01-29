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

#include <vector>

#include "shaka/media/text_track.h"
#include "src/debug/mutex.h"
#include "src/media/mse_media_player.h"

namespace shaka {
namespace media {

class DefaultMediaPlayer::Impl {
 public:
  Impl(ClientList* clients, VideoRenderer* video_renderer,
       AudioRenderer* audio_renderer)
      : mutex("DefaultMediaPlayer"),
        mse_player(clients, video_renderer, audio_renderer) {}

  Mutex mutex;
  MseMediaPlayer mse_player;
  std::vector<std::shared_ptr<TextTrack>> text_tracks_;
};

DefaultMediaPlayer::DefaultMediaPlayer(VideoRenderer* video_renderer,
                                       AudioRenderer* audio_renderer)
    : impl_(new Impl(GetClientList(), video_renderer, audio_renderer)) {}
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


std::vector<std::shared_ptr<TextTrack>> DefaultMediaPlayer::TextTracks() {
  std::unique_lock<Mutex> lock(impl_->mutex);
  return impl_->text_tracks_;
}

std::vector<std::shared_ptr<const TextTrack>> DefaultMediaPlayer::TextTracks()
    const {
  std::unique_lock<Mutex> lock(impl_->mutex);
  return {impl_->text_tracks_.begin(), impl_->text_tracks_.end()};
}

std::shared_ptr<TextTrack> DefaultMediaPlayer::AddTextTrack(
    TextTrackKind kind, const std::string& label, const std::string& language) {
  std::unique_lock<Mutex> lock(impl_->mutex);
  impl_->text_tracks_.emplace_back(new TextTrack(kind, label, language, ""));
  return impl_->text_tracks_.back();
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
