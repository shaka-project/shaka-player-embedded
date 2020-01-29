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

#include "shaka/media/proxy_media_player.h"

#include <cmath>
#include <vector>

#include "shaka/optional.h"
#include "src/debug/mutex.h"
#include "src/util/utils.h"

namespace shaka {
namespace media {

class ProxyMediaPlayer::Impl {
 public:
  Impl()
      : mutex("ProxyMediaPlayer"), player(nullptr), implementation(nullptr) {}

  void reset() {
    player = nullptr;
    fill_mode.reset();
    volume.reset();
    muted.reset();
    autoplay.reset();
    current_time.reset();
    duration.reset();
    playback_rate.reset();
  }

  SharedMutex mutex;
  ClientList clients;
  MediaPlayer* player;
  std::string key_system;
  eme::Implementation* implementation;

  optional<VideoFillMode> fill_mode;
  optional<double> volume;
  optional<bool> muted;
  optional<bool> autoplay;
  optional<double> current_time;
  optional<double> duration;
  optional<double> playback_rate;
};


ProxyMediaPlayer::ProxyMediaPlayer() : impl_(new Impl) {}
ProxyMediaPlayer::~ProxyMediaPlayer() {}

VideoPlaybackQuality ProxyMediaPlayer::VideoPlaybackQuality() const {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    return impl_->player->VideoPlaybackQuality();
  else
    return VideoPlaybackQuality();
}

void ProxyMediaPlayer::AddClient(Client* client) {
  impl_->clients.AddClient(client);
}

void ProxyMediaPlayer::RemoveClient(Client* client) {
  impl_->clients.RemoveClient(client);
}

std::vector<BufferedRange> ProxyMediaPlayer::GetBuffered() const {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    return impl_->player->GetBuffered();
  else
    return {};
}

VideoReadyState ProxyMediaPlayer::ReadyState() const {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    return impl_->player->ReadyState();
  else
    return VideoReadyState::NotAttached;
}

VideoPlaybackState ProxyMediaPlayer::PlaybackState() const {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    return impl_->player->PlaybackState();
  else
    return VideoPlaybackState::Detached;
}

std::vector<std::shared_ptr<TextTrack>> ProxyMediaPlayer::TextTracks() {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    return impl_->player->TextTracks();
  else
    return {};
}

std::vector<std::shared_ptr<const TextTrack>> ProxyMediaPlayer::TextTracks()
    const {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    return const_cast<const MediaPlayer*>(impl_->player)->TextTracks();
  else
    return {};
}

std::shared_ptr<TextTrack> ProxyMediaPlayer::AddTextTrack(
    TextTrackKind kind, const std::string& label, const std::string& language) {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    return impl_->player->AddTextTrack(kind, label, language);
  else
    return nullptr;
}


bool ProxyMediaPlayer::SetVideoFillMode(VideoFillMode mode) {
  std::unique_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player) {
    return impl_->player->SetVideoFillMode(mode);
  } else {
    impl_->fill_mode = mode;
    return true;
  }
}

uint32_t ProxyMediaPlayer::Width() const {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  return impl_->player ? impl_->player->Width() : 0;
}

uint32_t ProxyMediaPlayer::Height() const {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  return impl_->player ? impl_->player->Height() : 0;
}

double ProxyMediaPlayer::Volume() const {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    return impl_->player->Volume();
  else
    return impl_->volume.value_or(1);
}

void ProxyMediaPlayer::SetVolume(double volume) {
  std::unique_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    impl_->player->SetVolume(volume);
  else
    impl_->volume = volume;
}

bool ProxyMediaPlayer::Muted() const {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    return impl_->player->Muted();
  else
    return impl_->muted.value_or(false);
}

void ProxyMediaPlayer::SetMuted(bool muted) {
  std::unique_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    impl_->player->SetMuted(muted);
  else
    impl_->muted = muted;
}


void ProxyMediaPlayer::Play() {
  std::unique_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    impl_->player->Play();
  else
    impl_->autoplay = true;
}

void ProxyMediaPlayer::Pause() {
  std::unique_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    impl_->player->Pause();
  else
    impl_->autoplay = false;
}

double ProxyMediaPlayer::CurrentTime() const {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    return impl_->player->CurrentTime();
  else
    return impl_->current_time.value_or(0);
}

void ProxyMediaPlayer::SetCurrentTime(double time) {
  std::unique_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    impl_->player->SetCurrentTime(time);
  else
    impl_->current_time = time;
}

double ProxyMediaPlayer::Duration() const {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    return impl_->player->Duration();
  else
    return impl_->duration.value_or(HUGE_VAL);
}

void ProxyMediaPlayer::SetDuration(double duration) {
  std::unique_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    impl_->player->SetDuration(duration);
  else
    impl_->duration = duration;
}

double ProxyMediaPlayer::PlaybackRate() const {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    return impl_->player->PlaybackRate();
  else
    return impl_->playback_rate.value_or(1);
}

void ProxyMediaPlayer::SetPlaybackRate(double rate) {
  std::unique_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    impl_->player->SetPlaybackRate(rate);
  else
    impl_->playback_rate = rate;
}


bool ProxyMediaPlayer::AttachSource(const std::string& src) {
  MediaPlayer* player = CreateSource(src);
  if (!player)
    return false;

  std::unique_lock<SharedMutex> lock(impl_->mutex);
  SetFields(player);
  return true;
}

bool ProxyMediaPlayer::AttachMse() {
  MediaPlayer* player = CreateMse();
  if (!player)
    return false;

  std::unique_lock<SharedMutex> lock(impl_->mutex);
  SetFields(player);
  return true;
}

bool ProxyMediaPlayer::AddMseBuffer(const std::string& mime, bool is_video,
                                    const ElementaryStream* stream) {
  std::unique_lock<SharedMutex> lock(impl_->mutex);
  if (!impl_->player)
    return false;
  return impl_->player->AddMseBuffer(mime, is_video, stream);
}

void ProxyMediaPlayer::MseEndOfStream() {
  std::unique_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    impl_->player->MseEndOfStream();
}

void ProxyMediaPlayer::LoadedMetaData(double duration) {
  util::shared_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    impl_->player->LoadedMetaData(duration);
}


bool ProxyMediaPlayer::SetEmeImplementation(
    const std::string& key_system, eme::Implementation* implementation) {
  std::unique_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player) {
    if (!impl_->player->SetEmeImplementation(key_system, implementation))
      return false;
  }
  impl_->key_system = key_system;
  impl_->implementation = implementation;
  return true;
}

void ProxyMediaPlayer::Detach() {
  std::unique_lock<SharedMutex> lock(impl_->mutex);
  if (impl_->player)
    impl_->player->Detach();
  impl_->reset();
}

MediaPlayer::ClientList* ProxyMediaPlayer::GetClientList() const {
  return &impl_->clients;
}

void ProxyMediaPlayer::SetFields(MediaPlayer* player) {
  impl_->player = player;
  if (impl_->implementation)
    player->SetEmeImplementation(impl_->key_system, impl_->implementation);

  if (impl_->fill_mode.has_value())
    player->SetVideoFillMode(impl_->fill_mode.value());
  if (impl_->volume.has_value())
    player->SetVolume(impl_->volume.value());
  if (impl_->muted.has_value())
    player->SetMuted(impl_->muted.value());
  if (impl_->current_time.has_value())
    player->SetCurrentTime(impl_->current_time.value());
  if (impl_->duration.has_value())
    player->SetDuration(impl_->duration.value());
  if (impl_->playback_rate.has_value())
    player->SetPlaybackRate(impl_->playback_rate.value());
  if (impl_->autoplay.has_value()) {
    if (impl_->autoplay.value())
      player->Play();
    else
      player->Pause();
  }
}

}  // namespace media
}  // namespace shaka
