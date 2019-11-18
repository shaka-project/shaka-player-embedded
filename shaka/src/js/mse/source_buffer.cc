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

#include "src/js/mse/source_buffer.h"

#include <cmath>
#include <utility>

#include "src/js/events/event.h"
#include "src/js/events/event_names.h"
#include "src/js/js_error.h"
#include "src/js/mse/media_source.h"
#include "src/js/mse/time_ranges.h"
#include "src/media/media_utils.h"

namespace shaka {
namespace js {
namespace mse {

SourceBuffer::SourceBuffer(const std::string& mime,
                           RefPtr<MediaSource> media_source)
    : mode(AppendMode::SEGMENTS),
      updating(false),
      demuxer_(mime, media_source.get(), &frames_),
      media_source_(media_source),
      timestamp_offset_(0),
      append_window_start_(0),
      append_window_end_(HUGE_VAL /* Infinity */) {
  AddListenerField(EventType::UpdateStart, &on_update_start);
  AddListenerField(EventType::Update, &on_update);
  AddListenerField(EventType::UpdateEnd, &on_update_end);
  AddListenerField(EventType::Error, &on_error);
  AddListenerField(EventType::Abort, &on_abort);
}

// \cond Doxygen_Skip
SourceBuffer::~SourceBuffer() {}
// \endcond Doxygen_Skip

void SourceBuffer::Trace(memory::HeapTracer* tracer) const {
  events::EventTarget::Trace(tracer);
  tracer->Trace(&append_buffer_);
  tracer->Trace(&media_source_);
}

bool SourceBuffer::Attach(const std::string& mime, media::MediaPlayer* player,
                          bool is_video) {
  return player->AddMseBuffer(mime, is_video, &frames_);
}

void SourceBuffer::Detach() {
  demuxer_.Stop();
  media_source_ = nullptr;
}

ExceptionOr<void> SourceBuffer::AppendBuffer(ByteBuffer data) {
  if (!media_source_) {
    return JsError::DOMException(
        InvalidStateError, "SourceBuffer has been detached from the <video>.");
  }
  if (updating) {
    return JsError::DOMException(InvalidStateError,
                                 "Already performing an update.");
  }

  if (media_source_->ready_state == MediaSourceReadyState::ENDED) {
    media_source_->ready_state = MediaSourceReadyState::OPEN;
    media_source_->ScheduleEvent<events::Event>(EventType::SourceOpen);
  }

  append_buffer_ = std::move(data);
  demuxer_.AppendData(
      timestamp_offset_, append_window_start_, append_window_end_,
      append_buffer_.data(), append_buffer_.size(),
      std::bind(&SourceBuffer::OnAppendComplete, this, std::placeholders::_1));

  updating = true;
  return {};
}

void SourceBuffer::Abort() {
  // TODO:
}

ExceptionOr<void> SourceBuffer::Remove(double start, double end) {
  if (!media_source_) {
    return JsError::DOMException(
        InvalidStateError, "SourceBuffer has been detached from the <video>.");
  }
  if (updating) {
    return JsError::DOMException(InvalidStateError,
                                 "Already performing an update.");
  }

  // TODO: Consider running this on a background thread.
  frames_.Remove(start, end);

  ScheduleEvent<events::Event>(EventType::UpdateEnd);
  return {};
}

media::BufferedRanges SourceBuffer::GetBufferedRanges() const {
  return frames_.GetBufferedRanges();
}

ExceptionOr<RefPtr<TimeRanges>> SourceBuffer::GetBuffered() const {
  if (!media_source_) {
    return JsError::DOMException(
        InvalidStateError,
        "SourceBuffer is detached from the <video> element.");
  }
  return new TimeRanges(frames_.GetBufferedRanges());
}

double SourceBuffer::TimestampOffset() const {
  return timestamp_offset_;
}

ExceptionOr<void> SourceBuffer::SetTimestampOffset(double offset) {
  if (!std::isfinite(offset)) {
    return JsError::TypeError("timestampOffset cannot be NaN or +/-Infinity.");
  }

  if (!media_source_) {
    return JsError::DOMException(
        InvalidStateError,
        "SourceBuffer is detached from the <video> element.");
  }
  if (updating) {
    return JsError::DOMException(InvalidStateError,
                                 "Already performing an update.");
  }

  timestamp_offset_ = offset;
  return {};
}

double SourceBuffer::AppendWindowStart() const {
  return append_window_start_;
}

ExceptionOr<void> SourceBuffer::SetAppendWindowStart(double window_start) {
  if (!std::isfinite(window_start)) {
    return JsError::TypeError(
        "appendWindowStart cannot be NaN or +/-Infinity.");
  }

  if (!media_source_) {
    return JsError::DOMException(
        InvalidStateError,
        "SourceBuffer is detached from the <video> element.");
  }
  if (updating) {
    return JsError::DOMException(InvalidStateError,
                                 "Already performing an update.");
  }
  if (window_start < 0) {
    return JsError::TypeError("appendWindowStart cannot be negative.");
  }
  if (window_start >= append_window_end_) {
    return JsError::TypeError(
        "appendWindowStart cannot be greater than appendWindowEnd.");
  }

  append_window_start_ = window_start;
  return {};
}

double SourceBuffer::AppendWindowEnd() const {
  return append_window_end_;
}

ExceptionOr<void> SourceBuffer::SetAppendWindowEnd(double window_end) {
  if (!media_source_) {
    return JsError::DOMException(
        InvalidStateError,
        "SourceBuffer is detached from the <video> element.");
  }
  if (updating) {
    return JsError::DOMException(InvalidStateError,
                                 "Already performing an update.");
  }
  if (std::isnan(window_end)) {
    return JsError::TypeError("appendWindowEnd cannot be NaN.");
  }
  if (window_end <= append_window_start_) {
    return JsError::TypeError(
        "appendWindowEnd cannot be less than appendWindowStart.");
  }

  append_window_end_ = window_end;
  return {};
}

void SourceBuffer::OnAppendComplete(media::Status status) {
  VLOG(1) << "Finish appending media segment: " << status;
  updating = false;
  append_buffer_.Clear();
  if (status != media::Status::Success) {
    Abort();
    ScheduleEvent<events::Event>(EventType::Error);
  }
  ScheduleEvent<events::Event>(EventType::UpdateEnd);
}


SourceBufferFactory::SourceBufferFactory() {
  AddListenerField(EventType::UpdateStart, &SourceBuffer::on_update_start);
  AddListenerField(EventType::Update, &SourceBuffer::on_update);
  AddListenerField(EventType::UpdateEnd, &SourceBuffer::on_update_end);
  AddListenerField(EventType::Error, &SourceBuffer::on_error);
  AddListenerField(EventType::Abort, &SourceBuffer::on_abort);

  AddGenericProperty("buffered", &SourceBuffer::GetBuffered);

  AddGenericProperty("timestampOffset", &SourceBuffer::TimestampOffset,
                     &SourceBuffer::SetTimestampOffset);
  AddGenericProperty("appendWindowStart", &SourceBuffer::AppendWindowStart,
                     &SourceBuffer::SetAppendWindowStart);
  AddGenericProperty("appendWindowEnd", &SourceBuffer::AppendWindowEnd,
                     &SourceBuffer::SetAppendWindowEnd);

  AddReadWriteProperty("mode", &SourceBuffer::mode);
  AddReadOnlyProperty("updating", &SourceBuffer::updating);

  AddMemberFunction("appendBuffer", &SourceBuffer::AppendBuffer);
  AddMemberFunction("abort", &SourceBuffer::Abort);
  AddMemberFunction("remove", &SourceBuffer::Remove);

  NotImplemented("audioTracks");
  NotImplemented("videoTracks");
  NotImplemented("textTracks");
}

}  // namespace mse
}  // namespace js
}  // namespace shaka
