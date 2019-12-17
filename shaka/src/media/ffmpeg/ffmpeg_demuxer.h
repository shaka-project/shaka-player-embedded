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

#ifndef SHAKA_EMBEDDED_MEDIA_FFMPEG_FFMPEG_DEMUXER_H_
#define SHAKA_EMBEDDED_MEDIA_FFMPEG_FFMPEG_DEMUXER_H_

extern "C" {
#include <libavformat/avformat.h>
}

#include <memory>
#include <string>
#include <vector>

#include "shaka/media/demuxer.h"
#include "shaka/media/stream_info.h"
#include "src/debug/mutex.h"
#include "src/debug/thread.h"
#include "src/debug/thread_event.h"

namespace shaka {
namespace media {
namespace ffmpeg {

/**
 * An implementation of the Demuxer type that uses FFmpeg to demux frames.  This
 * produces FFmpegEncodedFrame objects.
 */
class FFmpegDemuxer : public Demuxer {
 public:
  FFmpegDemuxer(Demuxer::Client* client, const std::string& mime_type,
                const std::string& container);
  ~FFmpegDemuxer() override;

  void Reset() override;

  bool Demux(double timestamp_offset, const uint8_t* data, size_t size,
             std::vector<std::shared_ptr<EncodedFrame>>* frames) override;

 private:
  enum class State {
    Waiting,
    Parsing,
    Errored,
    Stopping,
  };
  template <typename T, void (*Free)(T**)>
  struct AVFree {
    void operator()(T* obj) {
      Free(&obj);
    }
  };
  using CodecParameters =
      std::unique_ptr<AVCodecParameters,
                      AVFree<AVCodecParameters, &avcodec_parameters_free>>;
  using FormatContext =
      std::unique_ptr<AVFormatContext,
                      AVFree<AVFormatContext, &avformat_close_input>>;

  static int OnRead(void* user, uint8_t* buffer, int size);

  void ThreadMain();

  bool ReinitDemuxer();
  void UpdateEncryptionInfo();
  void OnError();

  ThreadEvent<void> signal_;
  Mutex mutex_;
  const std::string mime_type_;
  const std::string container_;

  // These fields can only be used from the background thread and aren't
  // protected by the mutex.
  std::shared_ptr<const StreamInfo> cur_stream_info_;
  AVIOContext* io_;
  FormatContext demuxer_ctx_;
  Demuxer::Client* client_;

  // These fields are protected by the mutex.
  std::vector<std::shared_ptr<EncodedFrame>>* output_;
  double timestamp_offset_;
  const uint8_t* input_;
  size_t input_size_;
  size_t input_pos_;
  State state_;

  // Needs to be last so fields are initialized when the thread starts.
  Thread thread_;
};

class FFmpegDemuxerFactory : public DemuxerFactory {
 public:
  bool IsTypeSupported(const std::string& mime_type) const override;
  bool IsCodecVideo(const std::string& codec) const override;

  std::unique_ptr<Demuxer> Create(const std::string& mime_type,
                                  Demuxer::Client* client) const override;
};

}  // namespace ffmpeg
}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_FFMPEG_FFMPEG_DEMUXER_H_
