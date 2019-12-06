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

#ifndef SHAKA_EMBEDDED_MEDIA_MSE_MEDIA_PLAYER_H_
#define SHAKA_EMBEDDED_MEDIA_MSE_MEDIA_PLAYER_H_

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "shaka/eme/implementation.h"
#include "shaka/media/decoder.h"
#include "shaka/media/media_player.h"
#include "shaka/media/renderer.h"
#include "src/debug/mutex.h"
#include "src/media/decoder_thread.h"
#include "src/media/pipeline_manager.h"
#include "src/media/pipeline_monitor.h"

namespace shaka {
namespace media {

/**
 * Defines the default MediaSource-based MediaPlayer.  This can only handle MSE
 * playback and uses custom playback tracking.
 */
class MseMediaPlayer final : public MediaPlayer, DecoderThread::Client {
 public:
  MseMediaPlayer(VideoRenderer* video_renderer,
                 AudioRenderer* audio_renderer);
  ~MseMediaPlayer() override;

  void SetDecoders(Decoder* video_decoder, Decoder* audio_decoder);

  MediaCapabilitiesInfo DecodingInfo(
      const MediaDecodingConfiguration& config) const override;
  struct VideoPlaybackQuality VideoPlaybackQuality() const override;
  void AddClient(MediaPlayer::Client* client) override;
  void RemoveClient(MediaPlayer::Client* client) override;
  std::vector<BufferedRange> GetBuffered() const override;
  VideoReadyState ReadyState() const override;
  VideoPlaybackState PlaybackState() const override;
  std::vector<std::shared_ptr<TextTrack>> TextTracks() override;
  std::vector<std::shared_ptr<const TextTrack>> TextTracks() const override;
  std::shared_ptr<TextTrack> AddTextTrack(TextTrackKind kind,
                                          const std::string& label,
                                          const std::string& language) override;

  bool SetVideoFillMode(VideoFillMode mode) override;
  uint32_t Height() const override;
  uint32_t Width() const override;
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
  class Source final {
   public:
    explicit Source(MseMediaPlayer* player);
    ~Source();

    DecodedStream* GetDecodedStream();
    Decoder* GetDecoder() const;
    void SetDecoder(Decoder* decoder);

    std::vector<BufferedRange> GetBuffered() const;
    std::shared_ptr<DecodedFrame> GetFrame(double time) const;

    bool IsAttached() const;
    void Attach(const ElementaryStream* stream);
    void Detach();
    void OnSeek();
    void SetCdm(eme::Implementation* cdm);

   private:
    std::unique_ptr<Decoder> default_decoder_;

    DecodedStream decoded_frames_;
    DecoderThread decoder_thread_;
    const ElementaryStream* input_;

    Decoder* decoder_;
  };

  void OnStatusChanged(VideoPlaybackState status);
  void ReadyStateChanged(VideoReadyState ready_state);
  void OnSeek();
  void OnError() override;
  void OnWaitingForKey() override;

  mutable SharedMutex mutex_;
  PipelineManager pipeline_manager_;
  PipelineMonitor pipeline_monitor_;
  VideoPlaybackState old_state_;
  VideoReadyState ready_state_;

  Source video_;
  Source audio_;

  VideoRenderer* video_renderer_;
  AudioRenderer* audio_renderer_;
  std::unordered_set<MediaPlayer::Client*> clients_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_MSE_MEDIA_PLAYER_H_
