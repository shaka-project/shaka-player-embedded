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

#ifndef SHAKA_EMBEDDED_MEDIA_PROXY_MEDIA_PLAYER_H_
#define SHAKA_EMBEDDED_MEDIA_PROXY_MEDIA_PLAYER_H_

#include <memory>
#include <string>

#include "../macros.h"
#include "media_player.h"

namespace shaka {
namespace media {

/**
 * Defines a MediaPlayer instance that proxies to one or more MediaPlayer
 * instances based on what content is being loaded.  This will store the
 * defaults for the Set* methods and pass them to the chosen MediaPlayer once
 * content is loaded.
 *
 * @ingroup media
 */
class SHAKA_EXPORT ProxyMediaPlayer : public MediaPlayer {
 public:
  ProxyMediaPlayer();
  ~ProxyMediaPlayer() override;

  VideoPlaybackQualityNew VideoPlaybackQuality() const override;
  void AddClient(Client* client) override;
  void RemoveClient(Client* client) override;
  std::vector<BufferedRange> GetBuffered() const override;
  VideoReadyState ReadyState() const override;
  VideoPlaybackState PlaybackState() const override;

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

 private:
  /**
   * Gets a MediaPlayer implementation that is used to play MSE content.  It is
   * expected the state has been reset to the default and the returned object
   * will live until this object is destroyed or until a call to Detach.
   *
   * @return The resulting MediaPlayer implementation, or null on error or if it
   *   isn't supported.
   */
  virtual MediaPlayer* CreateMse() = 0;

  /**
   * Gets a MediaPlayer implementation that is used to play the given content.
   * This is only called for src= playback.  It is expected the state
   * has been reset to the default and the returned object will live until this
   * object is destroyed or until a call to Detach.
   *
   * @param src The URL to pull data from.
   * @return The resulting MediaPlayer implementation, or null on error or if it
   *   isn't supported.
   */
  virtual MediaPlayer* CreateSource(const std::string& src) = 0;

  void SetFields(MediaPlayer* player);

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_PROXY_MEDIA_MEDIA_PLAYER_H_
