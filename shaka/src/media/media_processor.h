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
  explicit MediaProcessor(const std::string& codec);
  virtual ~MediaProcessor();

  NON_COPYABLE_OR_MOVABLE_TYPE(MediaProcessor);

  //@{
  std::string codec() const;
  //@}

  /**
   * Performs any global initialization that is required (e.g. registering
   * codecs).  This can be called multiple times, but it must be called before
   * any media objects are created.
   */
  static void Initialize();

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
  virtual Status DecodeFrame(
      double cur_time, std::shared_ptr<EncodedFrame> frame,
      eme::Implementation* cdm,
      std::vector<std::shared_ptr<DecodedFrame>>* decoded);

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
