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

#ifndef SHAKA_EMBEDDED_MEDIA_DEFAULT_MEDIA_PLAYER_H_
#define SHAKA_EMBEDDED_MEDIA_DEFAULT_MEDIA_PLAYER_H_

#include <memory>

#include "../macros.h"
#include "decoder.h"
#include "proxy_media_player.h"
#include "renderer.h"

namespace shaka {
namespace media {

/**
 * Defines the default MediaPlayer implementation.  This handles the current
 * time tracking and defines interfaces to swap out decryption (through EME
 * implementations), decoding, and rendering.
 *
 * @ingroup media
 */
class SHAKA_EXPORT DefaultMediaPlayer final : public ProxyMediaPlayer {
 public:
  /**
   * Creates a new DefaultMediaPlayer instance that uses the given objects to
   * render the full-frames.  Both must be provided, but they may not be used
   * depending on the source content.
   *
   * @param video_renderer The renderer used to draw video frames.
   * @param audio_renderer The renderer used to play audio frames.
   */
  DefaultMediaPlayer(VideoRenderer* video_renderer,
                     AudioRenderer* audio_renderer);
  ~DefaultMediaPlayer() override;

  /**
   * Sets the decoders used to decode frames.  This is used for the duration of
   * playback and may require re-initialization to handle stream switching.
   *
   * The decoders must live as long as this object, or until a call to this
   * method.  Unless the default decoder was removed from the build, you can
   * pass nullptr to reset back to the built-in decoder.
   *
   * @param video_decoder The decoder instance used to decode video frames.
   * @param audio_decoder The decoder instance used to decode audio frames.
   */
  void SetDecoders(Decoder* video_decoder, Decoder* audio_decoder);

  MediaCapabilitiesInfo DecodingInfo(
      const MediaDecodingConfiguration& config) const override;

 private:
  MediaPlayer* CreateMse() override;
  MediaPlayer* CreateSource(const std::string& src) override;

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_DEFAULT_MEDIA_PLAYER_H_
