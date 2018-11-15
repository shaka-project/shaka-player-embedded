// Copyright 2017 Google LLC
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

#ifndef SHAKA_EMBEDDED_MEDIA_VIDEO_CONTROLLER_H_
#define SHAKA_EMBEDDED_MEDIA_VIDEO_CONTROLLER_H_

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "shaka/eme/configuration.h"
#include "shaka/eme/implementation.h"
#include "shaka/frame.h"
#include "src/debug/mutex.h"
#include "src/mapping/byte_buffer.h"
#include "src/mapping/struct.h"
#include "src/media/decoder_thread.h"
#include "src/media/demuxer_thread.h"
#include "src/media/media_processor.h"
#include "src/media/media_utils.h"
#include "src/media/pipeline_manager.h"
#include "src/media/pipeline_monitor.h"
#include "src/media/renderer.h"
#include "src/media/stream.h"
#include "src/media/types.h"
#include "src/util/macros.h"

namespace shaka {
namespace media {


/**
 * This is the backing logic for a video element.  This handles buffering,
 * seeking, playback, etc..  This gets "attached" to a video element when the
 * src attribute is set from JavaScript.  This can be detached by changing the
 * src or by calling load().
 *
 * The video element represents a renderable surface (e.g. a Gtk window),
 * while this object holds the logic for MSE and handling of data.
 *
 * Unlike a full implementation of video, this object will handle logic for
 * seeking and playback rates.  A more general video element might only use
 * MSE as a source of data and have the logic of playback position handled by
 * a more general video handler.  However, we bundle all the logic for playback
 * in this type.
 *
 * This object is owned by the MediaSource.  When the MediaSource gets attached
 * to a video element, the video will save a reference to the MediSource and
 * will also use this type.  This ensures that the MediaSource will remain
 * alive so long as there is either a reference to it or the video is playing.
 */
class VideoController {
 public:
  VideoController(std::function<void(SourceType, Status)> on_error,
                  std::function<void()> on_waiting_for_key,
                  std::function<void(eme::MediaKeyInitDataType, ByteBuffer)>
                      on_encrypted_init_data,
                  std::function<void(MediaReadyState)> on_ready_state_changed,
                  std::function<void(PipelineStatus)> on_pipeline_changed);
  ~VideoController();

  //@{
  /** @return The pipeline manager for this video. */
  const PipelineManager* GetPipelineManager() const {
    return &pipeline_;
  }
  PipelineManager* GetPipelineManager() {
    return &pipeline_;
  }
  //@}

  /** Sets the volume of the audio. */
  void SetVolume(double volume);

  /** Draws the current video frame onto a texture and returns it. */
  Frame DrawFrame(double* delay);
  /** Sets the CDM implementation used to decrypt media. */
  void SetCdm(eme::Implementation* cdm);

  Status AddSource(const std::string& mime_type, SourceType* source_type);
  /**
   * Appends the given data to the media source.  This assumes the data will
   * exist until on_complete is called.
   * TODO: Investigate using ref-counting to keep this alive.
   * @return False if the type wasn't found (or was detached), otherwise true.
   */
  bool AppendData(SourceType type, double timestamp_offset, double window_start,
                  double window_end, const uint8_t* data, size_t data_size,
                  std::function<void(Status)> on_complete);
  bool Remove(SourceType type, double start, double end);
  void EndOfStream();

  /** @return The current video quality info. */
  const VideoPlaybackQuality* GetVideoPlaybackQuality() const {
    return &quality_info_;
  }

  /**
   * Gets the buffered ranges for the given type.  If the type is Unknown, this
   * returns the intersection of the ranges.
   */
  BufferedRanges GetBufferedRanges(SourceType type) const;

  /**
   * Resets all data and clears all internal state.  This will reset the object
   * to when it was constructed.  This is NOT related to abort(), this is
   * called when the MediaSource gets closed (detached).
   */
  void Reset();


  /**
   * INTERNAL DEBUG USE ONLY.
   *
   * Dumps debug state to the console.  This includes current time, playback
   * rate, and buffered ranges.
   */
  void DebugDumpStats() const;

 private:
  struct Source {
    Source(
        SourceType source_type,
        PipelineManager* pipeline,
        const std::string& container,
        const std::string& codecs, std::function<void()> on_waiting_for_key,
        std::function<void(eme::MediaKeyInitDataType, const uint8_t*, size_t)>
            on_encrypted_init_data,
        std::function<double()> get_time,
        std::function<double()> get_playback_rate,
        std::function<void(Status)> on_error,
        std::function<void()> on_load_meta);
    ~Source();
    NON_COPYABLE_OR_MOVABLE_TYPE(Source);

    void OnSeekDone();

    MediaProcessor processor;
    Stream stream;
    DecoderThread decoder;
    DemuxerThread demuxer;
    std::unique_ptr<Renderer> renderer;
    bool ready;
  };

  Source* GetSource(SourceType type) const {
    return sources_.count(type) != 0 ? sources_.at(type).get() : nullptr;
  }

  void OnSeek();
  void OnLoadMeta(SourceType type);
  void OnError(SourceType type, Status error);
  void OnEncryptedInitData(eme::MediaKeyInitDataType init_data_type,
                           const uint8_t* data, size_t data_size);
  BufferedRanges GetDecodedRanges() const;
  double GetPlaybackRate() const;

  mutable SharedMutex mutex_;
  std::unordered_map<SourceType, std::unique_ptr<Source>> sources_;
  std::function<void(SourceType, Status)> on_error_;
  std::function<void()> on_waiting_for_key_;
  std::function<void(eme::MediaKeyInitDataType, ByteBuffer)>
      on_encrypted_init_data_;
  PipelineManager pipeline_;
  PipelineMonitor monitor_;
  VideoPlaybackQuality quality_info_;
  eme::Implementation* cdm_;
  double volume_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_VIDEO_CONTROLLER_H_
