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

#ifndef SHAKA_EMBEDDED_MEDIA_DECODER_H_
#define SHAKA_EMBEDDED_MEDIA_DECODER_H_

#include <memory>
#include <string>

#include "../eme/implementation.h"
#include "../macros.h"
#include "frames.h"
#include "media_capabilities.h"

namespace shaka {
namespace media {

/**
 * This is used by the DefaultMediaPlayer to decode EncodedFrame objects into
 * DecodedFrame objects.  If using a custom MediaPlayer, this type doesn't have
 * to be used.
 *
 * This object is used to decode a single stream type (e.g. audio or video), but
 * this needs to support switching between different streams for adaptation.
 *
 * With the exception of DecodingInfo, this is only used on a single background
 * thread.
 *
 * @ingroup media
 */
class SHAKA_EXPORT Decoder {
 public:
  Decoder();
  Decoder(const Decoder&) = delete;
  Decoder(Decoder&&) = delete;
  virtual ~Decoder();

  Decoder& operator=(const Decoder&) = delete;
  Decoder& operator=(Decoder&&) = delete;

  /** @see MediaPlayer::DecodingInfo */
  virtual MediaCapabilitiesInfo DecodingInfo(
      const MediaDecodingConfiguration& config) const = 0;

  /**
   * Resets any internal state due to a seek.  The next frame given will be a
   * keyframe.  This is not called for changing sub-streams.
   */
  virtual void ResetDecoder() = 0;

  /**
   * Attempts to decode the given frame into some number of full frames.
   *
   * This is given frames in DTS order, starting with a keyframe.  The caller
   * will call ResetDecoder if there is a seek before passing new frames.  This
   * may be given frames from different sub-streams, but changes will always
   * start with a keyframe.
   *
   * @param input The frame to decode.  This can be nullptr to flush the
   *   decoder.
   * @param eme The EME implementation used to decrypt frames, or nullptr if not
   *   using EME.
   * @param frames [OUT] Where to insert newly created frames.
   * @return The status of the decode operation.
   */
  virtual MediaStatus Decode(
      std::shared_ptr<EncodedFrame> input, const eme::Implementation* eme,
      std::vector<std::shared_ptr<DecodedFrame>>* frames) = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_DECODER_H_
