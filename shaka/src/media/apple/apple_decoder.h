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

#ifndef SHAKA_EMBEDDED_MEDIA_APPLE_DECODER_H_
#define SHAKA_EMBEDDED_MEDIA_APPLE_DECODER_H_

#include <AudioToolbox/AudioToolbox.h>
#include <VideoToolbox/VideoToolbox.h>

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "shaka/media/decoder.h"
#include "shaka/media/frames.h"
#include "shaka/media/stream_info.h"
#include "src/debug/mutex.h"
#include "src/media/apple/apple_decoded_frame.h"
#include "src/util/cfref.h"

namespace shaka {
namespace media {
namespace apple {

/**
 * An implementation of the Decoder type that uses AudioToolbox/VideoToolbox
 * decoder to decode frames.  This produces AppleDecodedFrame objects.
 */
class AppleDecoder : public Decoder {
 public:
  AppleDecoder();
  ~AppleDecoder() override;

  MediaCapabilitiesInfo DecodingInfo(
      const MediaDecodingConfiguration& config) const override;

  void ResetDecoder() override;

  MediaStatus Decode(
      std::shared_ptr<EncodedFrame> input, const eme::Implementation* eme,
      std::vector<std::shared_ptr<DecodedFrame>>* frames,
      std::string* extra_info) override;

 private:
  static void OnNewVideoFrame(void* user, void* frameUser, OSStatus status,
                              VTDecodeInfoFlags flags, CVImageBufferRef buffer,
                              CMTime pts, CMTime duration);
  static OSStatus AudioInputCallback(
      AudioConverterRef conv, UInt32* num_packets, AudioBufferList* data,
      AudioStreamPacketDescription** desc, void* user);

  void ResetInternal();

  bool DecodeVideo(const uint8_t* data, size_t data_size,
                   std::string* extra_info);
  bool DecodeAudio(const uint8_t* data, size_t data_size,
                   std::string* extra_info);

  bool InitVideoDecoder(std::shared_ptr<const StreamInfo> info,
                        std::string* extra_info);
  bool InitAudioDecoder(std::shared_ptr<const StreamInfo> info,
                        std::string* extra_info);

  Mutex mutex_;
  EncodedFrame* input_;
  const uint8_t* input_data_;
  size_t input_data_size_;
  std::vector<std::shared_ptr<DecodedFrame>>* output_;
  std::shared_ptr<const StreamInfo> decoder_stream_info_;

  util::CFRef<VTDecompressionSessionRef> vt_session_;
  util::CFRef<CMVideoFormatDescriptionRef> format_desc_;

  std::unique_ptr<std::remove_pointer<AudioConverterRef>::type,
                  OSStatus(*)(AudioConverterRef)>
      at_session_;
  AudioStreamPacketDescription audio_desc_;
};

}  // namespace apple
}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_APPLE_DECODER_H_
