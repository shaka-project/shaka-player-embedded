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

#ifndef SHAKA_EMBEDDED_MEDIA_MEDIA_PLAYER_H_
#define SHAKA_EMBEDDED_MEDIA_MEDIA_PLAYER_H_

#include <memory>
#include <string>
#include <vector>

#include "../eme/implementation.h"
#include "../macros.h"
#include "media_capabilities.h"
#include "streams.h"
#include "text_track.h"

namespace shaka {
namespace media {

/**
 * @defgroup media MediaPlayer Implementations
 * @ingroup exported
 * Interfaces for defining custom MediaPlayer implementations.
 */

/**
 * Defines possible fill modes for the video.  When drawing the video onto a
 * region, this determines how the video gets resized to fit.  The video frame
 * will always be centered within the region.
 *
 * @ingroup media
 */
enum class VideoFillMode : uint8_t {
  /**
   * Maintain the aspect ratio of the original video and size the video based on
   * the smaller of the extents.  There will be black bars around the video if
   * the region's aspect ratio isn't the same as the video's
   */
  MaintainRatio,

  /**
   * Draw the video frame the same as the original video.  This will put black
   * bars around the video if it's too small or will crop it if it's too big.
   */
  Original,

  /**
   * Stretch the video to completely fill the region.
   */
  Stretch,

  /**
   * Maintain the aspect ratio of the original video and size the video based on
   * the larger of the extents.  This will cause the video to be cropped to fit
   * in the region, but there won't be any black bars around the video.
   */
  Zoom,
};

/**
 * Defines possible content states the video can be in.  This defines how much
 * content is loaded around the current playhead time.  This is similar to the
 * "readyState" attribute from HTML.  Not all states need to be used and this is
 * mainly used to report to JavaScript.
 *
 * @ingroup media
 */
enum class VideoReadyState : int8_t {
  /**
   * There is no content and we haven't attached to a playback instance.  This
   * is negative so the other values have the same numerical values as
   * JavaScript.  This also preserves total ordering of the states where not
   * attached is less than attached with nothing loaded.
   */
  NotAttached = -1,

  /** Playback has been attached, but nothing has been loaded yet. */
  HaveNothing = 0,

  /** Playback has been attached and the metadata has been loaded. */
  HaveMetadata = 1,

  /** Playback has been attached and there is media data at the current time. */
  HaveCurrentData = 2,

  /**
   * Playback has been attached and there is media data at the current time and
   * up to a short time in the future.  Playback could move forward if playing.
   */
  HaveFutureData = 3,

  /**
   * Playback has been attached and there is media data at the current time and
   * up to a long time in the future.  Playback is expected to continue without
   * buffering.
   */
  HaveEnoughData = 4,
};

/**
 * Defines possible playback states the video can be in.  This defines how the
 * playhead is moving or why it isn't moving.
 *
 * @ingroup media
 */
enum class VideoPlaybackState : uint8_t {
  /** There is no playback attached. */
  Detached,

  /** Waiting for the initial segment data. */
  Initializing,

  /**
   * The video is paused by user action (i.e. the MediaPlayer::Pause method).
   */
  Paused,

  /**
   * The video is seeking to another time.  It will remain in this state until
   * content is available at the new time.
   */
  Seeking,

  /**
   * The video is waiting for new content; if there was content available, this
   * would be Playing.
   */
  Buffering,

  /**
   * The video is waiting for an encryption key; if the key was available, this
   * would be Playing.
   */
  WaitingForKey,

  /** The video is moving forward and playing content. */
  Playing,

  /**
   * The video has reached the end of the content.  This doesn't happen when
   * reaching the end of available content (i.e. the end of a buffer), but when
   * reaching the duration of the video.
   */
  Ended,
};

/**
 * Describes the current statistics about video playback quality.
 * @ingroup media
 */
struct VideoPlaybackQualityNew {
  /** The total number of video frames played. */
  uint32_t total_video_frames;

  /** The number of video frames that have been dropped. */
  uint32_t dropped_video_frames;

  /** The number of video frames that have been corrupted. */
  uint32_t corrupted_video_frames;
};


/**
 * Defines the base class for media handling.  This handles decryption,
 * decoding, playback, and rendering of media content.  This can handle both
 * MSE-based playback and raw src= playback; it is not required to support both,
 * returning <code>false</code> from the respective methods will stop playback.
 * For MSE-based playback, this is given the encoded frames after they have been
 * demuxed by the Demuxer.  This is expected to pull those frames when needed.
 *
 * This acts both as the <code>&lt;video&gt;</code> element to JavaScript and
 * as the app's API to interact with playback.  It is expected that this type
 * is internally thread-safe and can be called from multiple threads.
 *
 * An object of a derived class is created by the app during startup and is
 * given to the Player.  The Client is created by Shaka Player Embedded and is
 * given to this object.  Once the manifest is loaded, playback is started by
 * calling either AttachMse or AttachSource.  Once playback is done, Detach is
 * called.
 *
 * If another manifest is loaded, we will start again for a new stream.
 * Implementers are not required to support multiple playback sessions, but
 * they'll need to avoid calling Player::Load a second time with the same
 * MediaPlayer instance.
 *
 * @ingroup media
 */
class SHAKA_EXPORT MediaPlayer {
 public:
  /**
   * Defines an interface for listening for player events.  These callbacks are
   * invoked by the MediaPlayer when events happen.  These can be called on
   * any thread.
   *
   * These are called synchronously with a lock held on the MediaPlayer, so
   * you can't call back into the MediaPlayer instance from a callback.
   */
  class SHAKA_EXPORT Client {
   public:
    Client();
    Client(const Client&) = delete;
    Client(Client&&) = delete;
    virtual ~Client();

    Client& operator=(const Client&) = delete;
    Client& operator=(Client&&) = delete;

    /** Called when the VideoReadyState of the media changes. */
    virtual void OnReadyStateChanged(VideoReadyState old_state,
                                     VideoReadyState new_state) = 0;

    /** Called when the VideoPlaybackState of the media changes. */
    virtual void OnPlaybackStateChanged(VideoPlaybackState old_state,
                                        VideoPlaybackState new_state) = 0;

    /**
     * Called when an error happens during playback.
     * @param error Describes what went wrong, can be empty.
     */
    virtual void OnError(const std::string& error) = 0;

    /**
     * Called when the video starts playing after startup or a call to Pause.
     * This is different from entering the Playing state since this is only
     * called for autoplay or a call to Pause.
     */
    virtual void OnPlay() = 0;

    /**
     * Called when the video starts seeking.  This may be called multiple times
     * while in the Seeking state, if there are multiple seeks.
     */
    virtual void OnSeeking() = 0;

    /**
     * Called when the video stops playing due to lack of an encryption key.
     * This should only be called once for each missing key, but can be called
     * multiple times if new keys arrive but there still isn't the required key.
     */
    virtual void OnWaitingForKey() = 0;
  };

  MediaPlayer();
  MediaPlayer(const MediaPlayer&) = delete;
  MediaPlayer(MediaPlayer&&) = delete;
  virtual ~MediaPlayer();

  MediaPlayer& operator=(const MediaPlayer&) = delete;
  MediaPlayer& operator=(MediaPlayer&&) = delete;

  /**
   * Checks whether the given content can be played.  In general, for MSE
   * playback, this shouldn't check whether it can be demuxed, this should only
   * check whether the streams can be decoded.  The Demuxer should handle
   * whether it can be demuxed.
   *
   * The return value must be the same throughout playback and should be the
   * same for all MediaPlayer implementations that are used.  Since type
   * support checking is done before a specific MediaPlayer is selected, it is
   * expected that all MediaPlayer instances return the same value.  If this
   * isn't the case, it can cause runtime decoder errors since filtering is done
   * early.
   *
   * @param config The configuration to check support for.
   * @return The capabilities for the given configuration.
   */
  virtual MediaCapabilitiesInfo DecodingInfo(
      const MediaDecodingConfiguration& config) const = 0;

  /** @return The current video playback statistics. */
  virtual VideoPlaybackQualityNew VideoPlaybackQuality() const = 0;

  /**
   * Adds a new client listener.  The given object will be called when events
   * are raised.
   */
  virtual void AddClient(Client* client) = 0;

  /**
   * Removes a client listener.  The given client will no longer be called when
   * events happen.
   */
  virtual void RemoveClient(Client* client) = 0;

  /**
   * Gets the ranges of buffered content in the media.  For MSE playback, this
   * should use the ElementaryStream objects passed to this object; for src=
   * playback, this type will internally handle buffering.
   */
  virtual std::vector<BufferedRange> GetBuffered() const = 0;

  /** @return The current VideoReadyState of the media. */
  virtual VideoReadyState ReadyState() const = 0;

  /** @return The current VideoPlaybackSate of the media. */
  virtual VideoPlaybackState PlaybackState() const = 0;

  /** @return The current text tracks in the media. */
  virtual std::vector<std::shared_ptr<TextTrack>> TextTracks() = 0;

  /** @return The current text tracks in the media. */
  virtual std::vector<std::shared_ptr<const TextTrack>> TextTracks() const = 0;

  /**
   * Adds a new text track to the player.  This can return nullptr if this
   * isn't supported.
   *
   * @param kind The kind of the new track.
   * @param label A string label for the track.
   * @param language The language of the new track.
   * @return The new track object, or nullptr on error or if unsupported.
   */
  virtual std::shared_ptr<TextTrack> AddTextTrack(
      TextTrackKind kind, const std::string& label,
      const std::string& language) = 0;


  /** @name Rendering */
  //@{
  /**
   * Sets how to resize video frames within the drawing region.
   * @param mode The new method to resize the video frames.
   * @return True on success, false on error or if the mode isn't supported.
   */
  virtual bool SetVideoFillMode(VideoFillMode mode) = 0;

  /** @return The current width of the video frames, in pixels. */
  virtual uint32_t Width() const = 0;

  /** @return The current height of the video frames, in pixels. */
  virtual uint32_t Height() const = 0;

  /** @return The current volume [0, 1]. */
  virtual double Volume() const = 0;

  /** Sets the volume [0, 1] to render audio at. */
  virtual void SetVolume(double volume) = 0;

  /** @return Whether the audio is muted. */
  virtual bool Muted() const = 0;

  /** Sets whether the audio is muted. */
  virtual void SetMuted(bool muted) = 0;
  //@}


  /** @name Playback */
  //@{
  /**
   * Starts playback of the current content.  If this is called before
   * AttachSource/AttachMse, this should start playing when content is given.
   */
  virtual void Play() = 0;

  /**
   * Pauses playback of the current content.  If this is called before
   * AttachSource/AttachMse, this should not start playing when content is
   * given.
   */
  virtual void Pause() = 0;

  /** @return The current time of the video, or 0 if nothing is loaded. */
  virtual double CurrentTime() const = 0;

  /**
   * Seeks to a new position in the currently-playing stream.  Does nothing
   * if no content is loaded.
   *
   * @param time The presentation time to seek to.
   */
  virtual void SetCurrentTime(double time) = 0;

  /** @return The current duration of the media, or Infinity if unknown. */
  virtual double Duration() const = 0;

  /**
   * Sets the duration of the media.
   * @param duration The duration, in seconds, to set to.
   */
  virtual void SetDuration(double duration) = 0;

  /**
   * @return The current playback rate of the video, or 1 if nothing is loaded.
   */
  virtual double PlaybackRate() const = 0;

  /**
   * Sets the playback rate of the video.  Does nothing if no content is loaded.
   * @param rate The new playback rate.
   */
  virtual void SetPlaybackRate(double rate) = 0;
  //@}


  /**
   * @name Internal methods
   * These are called by Shaka Player Embedded to setup state.
   */
  //@{
  /**
   * Starts playback using the given src= URL.  The player should read data from
   * the given URL and play its content.
   *
   * @param src The URL to read from.
   * @return True on success, false on invalid URL or if src= isn't supported.
   */
  virtual bool AttachSource(const std::string& src) = 0;

  /**
   * Starts MSE-based playback.  At a later time, AddMseBuffer will be called to
   * give streams to pull from.
   *
   * @return True on success, false if MSE playback isn't supported.
   */
  virtual bool AttachMse() = 0;

  /**
   * Adds a new MSE buffer to pull frames from.  This can be called after normal
   * playback has started, but that doesn't have to be supported.  This will
   * be called for each SourceBuffer object created.  If the source content is
   * multiplexed, this will be called twice for the same input stream, but
   * will be given separate audio/video buffers.
   *
   * @param mime The full MIME type given from JavaScript.
   * @param is_video True if this is for a video stream, false for audio.
   * @param stream The stream containing the encoded frames.
   * @return True on success, false on error or unsupported.
   */
  virtual bool AddMseBuffer(const std::string& mime, bool is_video,
                            const ElementaryStream* stream) = 0;

  /**
   * For MSE playback, this indicates that the init segments for the input
   * streams have been received and have the given estimated duration.  If there
   * is more than one input stream, this is called once they all have been
   * loaded.
   *
   * @param duration The estimated duration of the stream based on the init
   *   segments; can be infinity if unknown.
   */
  virtual void LoadedMetaData(double duration) = 0;

  /**
   * For MSE playback, this indicates that the last segment has been handled and
   * the current buffered end represents the end of all the content.
   */
  virtual void MseEndOfStream() = 0;

  /**
   * Sets the EME implementation instance used to decrypt media.  This will
   * be set early in playback and won't be changed while playing.  This will be
   * called with nullptr to clear the EME implementation.
   *
   * @param key_system The name of the key system this is for.
   * @param implementation The implementation used to decrypt frames.
   * @return True on success, false on error or unsupported.
   */
  virtual bool SetEmeImplementation(const std::string& key_system,
                                    eme::Implementation* implementation) = 0;

  /**
   * Stops playback from the current media.  This should stop using the current
   * ElementaryStream objects and halt playback.
   */
  virtual void Detach() = 0;
  //@}
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_MEDIA_PLAYER_H_
