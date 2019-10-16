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

#ifndef SHAKA_EMBEDDED_MEDIA_MEDIA_PROCESSOR_H_
#define SHAKA_EMBEDDED_MEDIA_MEDIA_PROCESSOR_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "shaka/eme/configuration.h"
#include "shaka/media/frames.h"
#include "src/media/types.h"

namespace shaka {

namespace eme {
class Implementation;
}  // namespace eme

namespace media {

/**
 * Handles processing the media frames.  This includes demuxing media segments
 * and decoding frames.  This contains all the platform-specific code for
 * processing media.  This will handle processing of a single stream.  A new
 * instance should be created for different streams.
 *
 * Methods may spawn worker threads for parallelization, but all methods return
 * synchronously when work is complete.  Any callbacks will be serialized to
 * only be called on one thread at a time, but it may not be the thread that
 * called the method.
 *
 * This type is fully thread safe.  But be sure to read the comments on each
 * method for when each method can be called.
 */
class MediaProcessor {
 public:
  MediaProcessor(
      const std::string& container, const std::string& codec,
      std::function<void(eme::MediaKeyInitDataType, const uint8_t*, size_t)>
          on_encrypted_init_data);
  virtual ~MediaProcessor();

  NON_COPYABLE_OR_MOVABLE_TYPE(MediaProcessor);

  //@{
  /**
   * Gets the codec/container that this processor is using.  This should only be
   * used for debugging and not presented to the user.  This may be altered
   * (e.g. converted to FFmpeg expected format), so it may be different than
   * what was originally requested.
   */
  std::string container() const;
  std::string codec() const;
  //@}

  /**
   * Gets the duration of the media as defined by the most recent initialization
   * segment received.  This will return 0 if it is unknown or if we haven't
   * received an init segment yet.
   */
  double duration() const;

  /**
   * Performs any global initialization that is required (e.g. registering
   * codecs).  This can be called multiple times, but it must be called before
   * any media objects are created.
   */
  static void Initialize();


  /**
   * Initializes the demuxer.  This will block until the init segment is read
   * and processed.  This method will call |on_read| to get the init segment.
   * Then this will store |on_read| to be called later to read data.
   *
   * @param on_read A callback that will read from the source.
   * @param on_reset_read A callback that will be called to reset the current
   *   read position inside the current segment.  The caller should resend the
   *   data for the current segment on the next read.
   */
  virtual Status InitializeDemuxer(
      std::function<size_t(uint8_t*, size_t)> on_read,
      std::function<void()> on_reset_read);

  /**
   * Reads the next demuxed (encoded) frame.  This blocks until the next frame
   * is received, or EOF is reached.  This calls the |on_read| given to
   * InitializeDemuxer to read data.  This can only be called after
   * InitializeDemuxer returns.
   *
   * @param frame [OUT] Will contain the next demuxed frame.  Not changed on
   *   errors
   */
  virtual Status ReadDemuxedFrame(std::unique_ptr<BaseFrame>* frame);

  /**
   * Adds the given frame to the decoder and decodes it into full frames.  This
   * may return no frames or multiple because of dependent frames.  This can
   * only be called after InitializeDecoder returns.
   *
   * The frames MUST be given in DTS order.  This will discard any frames until
   * the first keyframe.  If there is a seek, call ResetDecoder before giving
   * the new frames.
   *
   * If there is a decoder error, it is invalid to decode any more frames.
   *
   * @param cur_time The current playback time.
   * @param frame The next encoded frame to decode.
   * @param cdm The CDM used to decrypt protected frames.
   * @param decoded [OUT] Will contain the decoded frames.
   * @return The error code, or Success.
   */
  virtual Status DecodeFrame(double cur_time, const BaseFrame* frame,
                             eme::Implementation* cdm,
                             std::vector<std::unique_ptr<BaseFrame>>* decoded);

  /** Sets the offset, in seconds, to adjust timestamps in the demuxer. */
  virtual void SetTimestampOffset(double offset);

  /**
   * Called when seeking to reset the decoder.  This is different than
   * adaptation since it will discard any un-flushed frames.
   */
  virtual void ResetDecoder();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_MEDIA_PROCESSOR_H_
