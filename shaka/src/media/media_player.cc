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

#include "shaka/media/media_player.h"

#include <atomic>

namespace shaka {
namespace media {

namespace {

std::atomic<const MediaPlayer*> player_{nullptr};

}  // namespace

MediaPlayer::Client::Client() {}
MediaPlayer::Client::~Client() {}

void MediaPlayer::Client::OnReadyStateChanged(VideoReadyState old_state,
                                              VideoReadyState new_state) {}

void MediaPlayer::Client::OnPlaybackStateChanged(VideoPlaybackState old_state,
                                                 VideoPlaybackState new_state) {
}

void MediaPlayer::Client::OnError(const std::string& error) {}

void MediaPlayer::Client::OnAttachMse() {}

void MediaPlayer::Client::OnAttachSource() {}

void MediaPlayer::Client::OnDetach() {}

void MediaPlayer::Client::OnPlay() {}

void MediaPlayer::Client::OnSeeking() {}

void MediaPlayer::Client::OnWaitingForKey() {}


MediaPlayer::MediaPlayer() {}
MediaPlayer::~MediaPlayer() {}

void MediaPlayer::SetMediaPlayerForSupportChecks(const MediaPlayer* player) {
  player_.store(player, std::memory_order_relaxed);
}

const MediaPlayer* MediaPlayer::GetMediaPlayerForSupportChecks() {
  return player_.load(std::memory_order_relaxed);
}

}  // namespace media
}  // namespace shaka
