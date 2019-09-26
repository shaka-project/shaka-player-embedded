// Copyright 2016 Google LLC
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

#ifndef SHAKA_EMBEDDED_VIDEO_H_
#define SHAKA_EMBEDDED_VIDEO_H_

#include <stdint.h>

#include <memory>

#include "frame.h"
#include "js_manager.h"
#include "macros.h"
#include "shaka_config.h"
#include "text_track.h"

namespace shaka {

namespace js {
namespace mse {
class HTMLVideoElement;
}  // namespace mse
}  // namespace js


/**
 * This manages both a native "video" element and the JavaScript object that
 * uses it.  This will create a native implementation of a "video" element
 * and pass it to a created JavaScript object that implements the
 * HTMLVideoElement type.
 *
 * @ingroup player
 */
class SHAKA_EXPORT Video final {
 public:
  /**
   * Defines an interface for listening for Video events.  These callbacks are
   * invoked on a background thread by the Video object.
   */
  class Client {
   public:
    Client();
    Client(const Client&);
    Client(Client&&);
    virtual ~Client();

    Client& operator=(const Client&);
    Client& operator=(Client&&);

    /**
     * Called when the video starts playing after startup or a call to Pause().
     */
    virtual void OnPlaying();

    /**
     * Called when the video gets paused due to a call to Pause().
     */
    virtual void OnPause();

    /**
     * Called when the video plays to the end of the content.
     */
    virtual void OnEnded();


    /**
     * Called when the video starts seeking.  This may be called multiple times
     * in a row due to Shaka Player repositioning the playhead.
     */
    virtual void OnSeeking();

    /**
     * Called when the video completes seeking.  This happens once content is
     * available and the playhead can move forward.
     */
    virtual void OnSeeked();
  };

  /**
   * Creates a new Video instance.
   * @param engine The JavaScript engine to use.
   */
  Video(JsManager* engine);
  Video(const Video&) = delete;
  Video(Video&&);
  ~Video();

  Video& operator=(const Video&) = delete;
  Video& operator=(Video&&);


  /**
   * Initializes the video element.  This must be called once before any other
   * methods are called and before passing to Player.Initialize.
   *
   * @param client The client object that listens to events.
   */
  void Initialize(Client* client = nullptr);

  /**
   * Draws the current video frame onto a texture and returns it.  This
   * can be called on any thread, but cannot be called at the same time on
   * multiple threads.  The texture will be the same size of the video to avoid
   * resizing.  Resizing and adding black bars is the job of the app.
   *
   * @param delay [OUT] Optional, if given, will hold the delay (in seconds)
   *   until the next frame should be rendered.
   * @return The texture holding the video frame, or an invalid frame if nothing
   *   is ready.
   */
  Frame DrawFrame(double* delay);


  /** @return The duration of the video, or 0 if nothing is loaded. */
  double Duration() const;

  /** @return Whether the video is currently ended. */
  bool Ended() const;

  /** @return Whether the video is currently seeking. */
  bool Seeking() const;

  /** @return Whether the video is currently paused. */
  bool Paused() const;

  /** @return Whether the audio is currently muted. */
  bool Muted() const;

  /** Sets whether the audio is muted. */
  void SetMuted(bool muted);

  /** @return The text tracks of the video. */
  std::vector<TextTrack> TextTracks();

  /** @return The current volume of the audio. */
  double Volume() const;

  /** Sets the audio volume. */
  void SetVolume(double volume);

  /** @return The current time of the video, or 0 if nothing is loaded. */
  double CurrentTime() const;

  /**
   * Seeks to a new position in the currently-playing stream.  Does nothing
   * if no content is loaded.
   *
   * @param time The presentation time to seek to.
   */
  void SetCurrentTime(double time);

  /**
   * @return The current playback rate of the video, or 1 if nothing is loaded.
   */
  double PlaybackRate() const;

  /**
   * Sets the playback rate of the video.  Does nothing if no content is loaded.
   * @param rate The new playback rate.
   */
  void SetPlaybackRate(double rate);


  /** Pauses the video. */
  void Pause();

  /** Plays the video. */
  void Play();

 private:
  friend class Player;
  js::mse::HTMLVideoElement* GetJavaScriptObject();

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_VIDEO_H_
