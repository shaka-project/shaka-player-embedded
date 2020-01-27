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
#include <vector>

#include "src/debug/mutex.h"
#include "src/util/utils.h"

namespace shaka {
namespace media {

namespace {

std::atomic<const MediaPlayer*> player_{nullptr};

}  // namespace

MediaPlayer::Client::Client() {}
MediaPlayer::Client::~Client() {}

void MediaPlayer::Client::OnAddTextTrack(std::shared_ptr<TextTrack> track) {}

void MediaPlayer::Client::OnRemoveTextTrack(std::shared_ptr<TextTrack> track) {}

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


class MediaPlayer::ClientList::Impl {
 public:
  template <typename Func, typename... Args>
  void CallClientMethod(Func func, Args&&... args) {
    util::shared_lock<SharedMutex> lock(mutex);
    for (auto* client : clients)
      (client->*func)(std::forward<Args>(args)...);
  }

  SharedMutex mutex{"ClientList"};
  std::vector<Client*> clients;
};

MediaPlayer::ClientList::ClientList() : impl_(new Impl) {}
MediaPlayer::ClientList::~ClientList() {}

void MediaPlayer::ClientList::AddClient(Client* client) {
  std::unique_lock<SharedMutex> lock(impl_->mutex);
  if (!util::contains(impl_->clients, client))
    impl_->clients.emplace_back(client);
}

void MediaPlayer::ClientList::RemoveClient(Client* client) {
  std::unique_lock<SharedMutex> lock(impl_->mutex);
  util::RemoveElement(&impl_->clients, client);
}

void MediaPlayer::ClientList::OnAddTextTrack(std::shared_ptr<TextTrack> track) {
  impl_->CallClientMethod(&Client::OnAddTextTrack, track);
}

void MediaPlayer::ClientList::OnRemoveTextTrack(
    std::shared_ptr<TextTrack> track) {
  impl_->CallClientMethod(&Client::OnRemoveTextTrack, track);
}

void MediaPlayer::ClientList::OnReadyStateChanged(VideoReadyState old_state,
                                                  VideoReadyState new_state) {
  impl_->CallClientMethod(&Client::OnReadyStateChanged, old_state, new_state);
}

void MediaPlayer::ClientList::OnPlaybackStateChanged(
    VideoPlaybackState old_state, VideoPlaybackState new_state) {
  impl_->CallClientMethod(&Client::OnPlaybackStateChanged, old_state,
                          new_state);
}

void MediaPlayer::ClientList::OnError(const std::string& error) {
  impl_->CallClientMethod(&Client::OnError, error);
}

void MediaPlayer::ClientList::OnAttachMse() {
  impl_->CallClientMethod(&Client::OnAttachMse);
}

void MediaPlayer::ClientList::OnAttachSource() {
  impl_->CallClientMethod(&Client::OnAttachSource);
}

void MediaPlayer::ClientList::OnDetach() {
  impl_->CallClientMethod(&Client::OnDetach);
}

void MediaPlayer::ClientList::OnPlay() {
  impl_->CallClientMethod(&Client::OnPlay);
}

void MediaPlayer::ClientList::OnSeeking() {
  impl_->CallClientMethod(&Client::OnSeeking);
}

void MediaPlayer::ClientList::OnWaitingForKey() {
  impl_->CallClientMethod(&Client::OnWaitingForKey);
}


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
