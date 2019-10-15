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

#ifndef SHAKA_EMBEDDED_MEDIA_RENDERER_H_
#define SHAKA_EMBEDDED_MEDIA_RENDERER_H_

#include "../macros.h"
#include "streams.h"
#include "media_player.h"

namespace shaka {
namespace media {

/**
 * Defines an interface for rendering.  This type will handle pulling frames
 * from a DecodedStream object and rendering them to their destination.  This
 * is expected to periodically pull frames as needed to render, which may
 * require spawning background threads.
 *
 * Methods on this object should not be called by the app; these will be
 * handled by the DefaultMediaPlayer.  Methods on this object can be called from
 * any thread.
 *
 * TODO(modmaker): Rename to Renderer once old type is removed.
 *
 * @ingroup media
 */
class SHAKA_EXPORT RendererNew {
 public:
  RendererNew();
  RendererNew(const RendererNew&) = delete;
  RendererNew(RendererNew&&) = delete;
  virtual ~RendererNew();

  RendererNew& operator=(const RendererNew&) = delete;
  RendererNew& operator=(RendererNew&&) = delete;

  /** Called when a seek begins. */
  virtual void OnSeek() = 0;


  /**
   * Sets the MediaPlayer that is controlling this renderer.  This will be used
   * by the Renderer to query the current time and playback state.
   *
   * @param player The MediaPlayer instance controlling this object.
   */
  virtual void SetPlayer(const MediaPlayer* player) = 0;

  /**
   * Attaches to the given stream.  This object will now pull full-frames from
   * the given stream to play content.  The stream will live as long as this
   * object, or until a call to Detach.
   *
   * @param stream The stream to pull frames from.
   */
  virtual void Attach(const DecodedStream* stream) = 0;

  /**
   * Detaches playback from the current stream.  The current stream will no
   * longer be used to play content.
   */
  virtual void Detach() = 0;
};

/**
 * Defines a Renderer that handles audio rendering.
 * @ingroup media
 */
class SHAKA_EXPORT AudioRendererNew : public RendererNew {
 public:
  /** @return The current volume [0, 1]. */
  virtual double Volume() const = 0;

  /** Sets the volume [0, 1] to render audio at. */
  virtual void SetVolume(double volume) = 0;

  /** @return Whether the audio is muted. */
  virtual bool Muted() const = 0;

  /** Sets whether the audio is muted. */
  virtual void SetMuted(bool muted) = 0;
};

/**
 * Defines a Renderer that handles video rendering.
 * @ingroup media
 */
class SHAKA_EXPORT VideoRendererNew : public RendererNew {
 public:
  /** @see MediaPlayer::VideoPlaybackQuality */
  virtual VideoPlaybackQualityNew VideoPlaybackQuality() const = 0;

  /** @see MediaPlayer::SetVideoFillMode */
  virtual bool SetVideoFillMode(VideoFillMode mode) = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_RENDERER_H_
