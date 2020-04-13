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

#ifndef SHAKA_EMBEDDED_MEDIA_IOS_AV_MEDIA_PLAYER_H_
#define SHAKA_EMBEDDED_MEDIA_IOS_AV_MEDIA_PLAYER_H_

#include <memory>
#include <string>
#include <vector>

#include "shaka/media/default_media_player.h"
#include "shaka/media/media_player.h"

namespace shaka {
namespace media {
namespace ios {

/**
 * A MediaPlayer implementation that uses iOS' AVPlayer to play src= content.
 */
class AvMediaPlayer : public MediaPlayer {
 public:
  explicit AvMediaPlayer(ClientList* clients);
  ~AvMediaPlayer() override;

  const void* GetIosView();
  const void* GetAvPlayer();

  MediaCapabilitiesInfo DecodingInfo(
      const MediaDecodingConfiguration& config) const override;
  struct VideoPlaybackQuality VideoPlaybackQuality() const override;
  void AddClient(Client* client) const override;
  void RemoveClient(Client* client) const override;

  std::vector<BufferedRange> GetBuffered() const override;
  VideoReadyState ReadyState() const override;
  VideoPlaybackState PlaybackState() const override;
  std::vector<std::shared_ptr<MediaTrack>> AudioTracks() override;
  std::vector<std::shared_ptr<const MediaTrack>> AudioTracks() const override;
  std::vector<std::shared_ptr<MediaTrack>> VideoTracks() override;
  std::vector<std::shared_ptr<const MediaTrack>> VideoTracks() const override;
  std::vector<std::shared_ptr<TextTrack>> TextTracks() override;
  std::vector<std::shared_ptr<const TextTrack>> TextTracks() const override;
  std::shared_ptr<TextTrack> AddTextTrack(
      TextTrackKind kind, const std::string& label,
      const std::string& language) override;

  bool SetVideoFillMode(VideoFillMode mode) override;
  uint32_t Width() const override;
  uint32_t Height() const override;
  double Volume() const override;
  void SetVolume(double volume) override;
  bool Muted() const override;
  void SetMuted(bool muted) override;

  void Play() override;
  void Pause() override;
  double CurrentTime() const override;
  void SetCurrentTime(double time) override;
  double Duration() const override;
  void SetDuration(double duration) override;
  double PlaybackRate() const override;
  void SetPlaybackRate(double rate) override;

  bool AttachSource(const std::string& src) override;
  bool AttachMse() override;
  bool AddMseBuffer(const std::string& mime, bool is_video,
                    const ElementaryStream* stream) override;
  void LoadedMetaData(double duration) override;
  void MseEndOfStream() override;
  bool SetEmeImplementation(const std::string& key_system,
                            eme::Implementation* implementation) override;
  void Detach() override;

  class Impl;

 private:
  std::unique_ptr<Impl> impl_;
};

}  // namespace ios
}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_IOS_AV_MEDIA_PLAYER_H_
