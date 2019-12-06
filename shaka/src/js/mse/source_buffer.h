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

#ifndef SHAKA_EMBEDDED_JS_MSE_SOURCE_BUFFER_H_
#define SHAKA_EMBEDDED_JS_MSE_SOURCE_BUFFER_H_

#include <string>

#include "shaka/media/demuxer.h"
#include "shaka/media/media_player.h"
#include "shaka/media/streams.h"
#include "src/core/member.h"
#include "src/js/events/event_target.h"
#include "src/mapping/byte_buffer.h"
#include "src/mapping/enum.h"
#include "src/mapping/exception_or.h"
#include "src/media/demuxer_thread.h"
#include "src/media/types.h"

namespace shaka {
namespace js {
namespace mse {
class MediaSource;
class TimeRanges;


enum class AppendMode {
  SEGMENTS,
  SEQUENCE,
};

class SourceBuffer : public events::EventTarget {
  DECLARE_TYPE_INFO(SourceBuffer);

 public:
  SourceBuffer(const std::string& mime, RefPtr<MediaSource> media_source);

  void Trace(memory::HeapTracer* tracer) const override;

  bool Attach(const std::string& mime, media::MediaPlayer* player,
              bool is_video);
  void Detach();

  ExceptionOr<void> AppendBuffer(ByteBuffer data);
  void Abort();
  ExceptionOr<void> Remove(double start, double end);

  media::BufferedRanges GetBufferedRanges() const;
  ExceptionOr<RefPtr<TimeRanges>> GetBuffered() const;

  double TimestampOffset() const;
  ExceptionOr<void> SetTimestampOffset(double offset);
  double AppendWindowStart() const;
  ExceptionOr<void> SetAppendWindowStart(double window_start);
  double AppendWindowEnd() const;
  ExceptionOr<void> SetAppendWindowEnd(double window_end);

  AppendMode mode;
  bool updating;

  Listener on_update_start;
  Listener on_update;
  Listener on_update_end;
  Listener on_error;
  Listener on_abort;

 private:
  /** Called when an append operation completes. */
  void OnAppendComplete(bool success);

  media::ElementaryStream frames_;
  media::DemuxerThread demuxer_;

  Member<MediaSource> media_source_;
  ByteBuffer append_buffer_;
  double timestamp_offset_;
  double append_window_start_;
  double append_window_end_;
};

class SourceBufferFactory
    : public BackingObjectFactory<SourceBuffer, events::EventTarget> {
 public:
  SourceBufferFactory();
};

}  // namespace mse
}  // namespace js
}  // namespace shaka

DEFINE_ENUM_MAPPING(shaka::js::mse, AppendMode) {
  AddMapping(Enum::SEGMENTS, "segments");
  AddMapping(Enum::SEQUENCE, "sequence");
}

#endif  // SHAKA_EMBEDDED_JS_MSE_SOURCE_BUFFER_H_
