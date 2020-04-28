// Copyright 2020 Google LLC
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

#ifndef SHAKA_EMBEDDED_MEDIA_AUDIO_RENDERER_COMMON_H_
#define SHAKA_EMBEDDED_MEDIA_AUDIO_RENDERER_COMMON_H_

#include <memory>

#include "shaka/media/frames.h"
#include "shaka/media/renderer.h"
#include "src/debug/mutex.h"
#include "src/debug/thread.h"
#include "src/debug/thread_event.h"
#include "src/util/buffer_writer.h"
#include "src/util/clock.h"

namespace shaka {
namespace media {

/**
 * Holds common code between our AudioRenderer types.  This handles time
 * tracking, choosing the correct frames, and A/V sync.  The derived
 * class must implement the pure-virtual functions to interact with the audio
 * device.
 *
 * An audio device is something that plays from a single buffer of samples.
 * Samples are appended to the end of the buffer and are played at a constant
 * rate (i.e. the sample rate).  We fill the buffer ahead of what is actually
 * being played, so we need to predict which frames need to be played.  This
 * class handles the synchronization and predictions, the derived classes handle
 * how to talk to the device.
 *
 * When we are paused or seek, we need to resynchronize.  This will pick the
 * current frame based on the current time and start filling there.  From that
 * point forward, we will just append new frames sequentially.  This will also
 * handle the unlikely case of not having enough data or too much data to match
 * the frame times.
 *
 * This renderer only supports playing content the audio device natively
 * supports.  This cannot convert to a different sample format.
 *
 * This type is fully thread-safe; all the pure-virtual methods are called with
 * a lock held, so derived classes do not need to use locks.
 */
class AudioRendererCommon : public AudioRenderer, MediaPlayer::Client {
 public:
  AudioRendererCommon();
  ~AudioRendererCommon() override;

  void SetPlayer(const MediaPlayer* player) override;
  void Attach(const DecodedStream* stream) override;
  void Detach() override;

  double Volume() const override;
  void SetVolume(double volume) override;
  bool Muted() const override;
  void SetMuted(bool muted) override;

 protected:
  /**
   * Stops the internal thread.  This needs to be called in the derived class'
   * destructor so this doesn't try to reset the device while destroying.
   */
  void Stop();

 private:
  enum class SyncStatus {
    Success,
    NoFrame,
    FatalError,
  };

  /**
   * Initializes the audio device for playback of audio similar to the given
   * frame.  The device should start paused.
   *
   * If the audio device is already initialized, this must reset it first.  If
   * there is any buffered audio, that should be dropped.
   *
   * @param frame The frame to initialize for.
   * @param volume The current effective volume.
   * @return True on success, false on error.  If this returns an error, it is
   *   assumed to be fatal and audio won't be played anymore.
   */
  virtual bool InitDevice(std::shared_ptr<DecodedFrame> frame,
                          double volume) = 0;

  /**
   * Appends the given data to the end of the audio buffer.  It is assumed the
   * data is copied into the buffer.
   *
   * @param data The data to buffer.
   * @param size The number of bytes in data.
   * @return True on success, false on error.
   */
  virtual bool AppendBuffer(const uint8_t* data, size_t size) = 0;

  /** Clears any already-buffered audio data in the device. */
  virtual void ClearBuffer() = 0;

  /**
   * Returns the number of bytes that are currently buffered.  It is preferred
   * to return the exact value based on what the hardware is playing; but this
   * may only return the amount internally buffered.
   */
  virtual size_t GetBytesBuffered() const = 0;

  /** Changes whether the device is playing or paused. */
  virtual void SetDeviceState(bool is_playing) = 0;

  /**
   * Updates the volume of the audio device (0-1).  This is called with the
   * effective volume (i.e. this is 0 when muted).  If there is no audio device,
   * this does nothing.
   */
  virtual void UpdateVolume(double volume) = 0;

  /** Fills the audio device with the given number of bytes of silence. */
  bool FillSilence(size_t bytes);

  /**
   * Determines if the given frames are similar enough to use the same audio
   * device.  If this returns false, the audio device needs to be reset to
   * continue playing.
   */
  bool IsFrameSimilar(std::shared_ptr<DecodedFrame> frame1,
                      std::shared_ptr<DecodedFrame> frame2) const;

  void SetClock(const util::Clock* clock);

  void ThreadMain();


  void OnPlaybackStateChanged(VideoPlaybackState old_state,
                              VideoPlaybackState new_state) override;
  void OnPlaybackRateChanged(double old_rate, double new_rate) override;
  void OnSeeking() override;


  friend class AudioRendererCommonTest;
  mutable Mutex mutex_;
  ThreadEvent<void> on_play_;

  const util::Clock* clock_;
  const MediaPlayer* player_;
  const DecodedStream* input_;

  std::shared_ptr<DecodedFrame> cur_frame_;
  double sync_time_;
  uint64_t bytes_written_;
  double volume_;
  bool muted_;
  bool needs_resync_;
  bool shutdown_;

  Thread thread_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_AUDIO_RENDERER_COMMON_H_
