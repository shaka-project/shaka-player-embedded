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

#include "src/js/mse/media_source.h"

#include <glog/logging.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

#include "shaka/media/demuxer.h"
#include "src/js/events/event.h"
#include "src/js/events/event_names.h"
#include "src/js/events/media_encrypted_event.h"
#include "src/js/mse/source_buffer.h"
#include "src/js/mse/video_element.h"
#include "src/media/media_utils.h"
#include "src/util/macros.h"

namespace shaka {
namespace js {
namespace mse {

namespace {

/** @returns A random blob URL using a randomly generated UUID. */
std::string RandomUrl() {
  unsigned char bytes[16];
  // Use pseudo-random since we don't need cryptographic security.
  for (unsigned char& chr : bytes)
    chr = static_cast<unsigned char>(rand());  // NOLINT

    // Since it's random, don't care about host byte order.
#define _2_BYTES_AT(b) (*reinterpret_cast<uint16_t*>(b))
#define _4_BYTES_AT(b) (*reinterpret_cast<uint32_t*>(b))

  return util::StringPrintf(
      "blob:%08x-%04x-%04x-%04x-%08x%04x", _4_BYTES_AT(bytes),
      _2_BYTES_AT(bytes + 4),
      // Only output 3 hex chars, the first is the UUID version (4, Random).
      (_2_BYTES_AT(bytes + 6) & 0xfff) | 0x4000,
      // Drop the two high bits to set the variant (0b10xx).
      (_2_BYTES_AT(bytes + 8) & 0x3fff) | 0x8000, _4_BYTES_AT(bytes + 10),
      _2_BYTES_AT(bytes + 14));
}

}  // namespace

using namespace std::placeholders;  // NOLINT

BEGIN_ALLOW_COMPLEX_STATICS
std::unordered_map<std::string, Member<MediaSource>>
    MediaSource::media_sources_;
END_ALLOW_COMPLEX_STATICS

MediaSource::MediaSource()
    : ready_state(MediaSourceReadyState::CLOSED),
      url(RandomUrl()),
      player_(nullptr),
      got_loaded_metadata_(false) {
  AddListenerField(EventType::SourceOpen, &on_source_open);
  AddListenerField(EventType::SourceEnded, &on_source_ended);
  AddListenerField(EventType::SourceClose, &on_source_close);

  DCHECK_EQ(0u, media_sources_.count(url));
  media_sources_[url] = this;
}

// \cond Doxygen_Skip
MediaSource::~MediaSource() {
  DCHECK_EQ(1u, media_sources_.count(url));
  media_sources_.erase(url);
}
// \endcond Doxygen_Skip

// static
bool MediaSource::IsTypeSupported(const std::string& mime_type) {
  auto* player = media::MediaPlayer::GetMediaPlayerForSupportChecks();
  if (!player)
    player = HTMLVideoElement::AnyMediaPlayer();
  if (!player) {
    LOG(ERROR) << "Unable to find a MediaPlayer instance to query";
    return false;
  }

  auto info = ConvertMimeToDecodingConfiguration(
      mime_type, media::MediaDecodingType::MediaSource);
  auto support = player->DecodingInfo(info);
  return support.supported;
}

// static
RefPtr<MediaSource> MediaSource::FindMediaSource(const std::string& url) {
  if (media_sources_.count(url) == 0)
    return nullptr;
  return media_sources_[url];
}

void MediaSource::Trace(memory::HeapTracer* tracer) const {
  EventTarget::Trace(tracer);
  tracer->Trace(&audio_buffer_);
  tracer->Trace(&video_buffer_);
  tracer->Trace(&video_);
}

ExceptionOr<RefPtr<SourceBuffer>> MediaSource::AddSourceBuffer(
    const std::string& type) {
  if (ready_state != MediaSourceReadyState::OPEN) {
    return JsError::DOMException(
        InvalidStateError,
        R"(Cannot call addSourceBuffer() unless MediaSource is "open".)");
  }
  DCHECK(player_);

  std::unordered_map<std::string, std::string> params;
  if (!media::ParseMimeType(type, nullptr, nullptr, &params)) {
    return JsError::DOMException(
        NotSupportedError,
        "The given type ('" + type + "') is not a valid MIME type.");
  }
  const std::string codecs = params[media::kCodecMimeParam];

  auto* factory = media::DemuxerFactory::GetFactory();
  if (!factory) {
    return JsError::DOMException(NotSupportedError,
                                 "No Demuxer implementation provided");
  }
  if (!factory->IsTypeSupported(type) || codecs.empty() ||
      codecs.find(',') != std::string::npos) {
    return JsError::DOMException(
        NotSupportedError, "The given type ('" + type + "') is unsupported.");
  }

  const bool is_video = factory->IsCodecVideo(codecs);
  RefPtr<SourceBuffer> ret = new SourceBuffer(type, this);
  RefPtr<SourceBuffer> existing = is_video ? video_buffer_ : audio_buffer_;
  if (!existing.empty()) {
    return JsError::DOMException(QuotaExceededError,
                                 "Invalid SourceBuffer configuration");
  }

  if (!ret->Attach(type, player_, is_video)) {
    return JsError::DOMException(UnknownError, "Error attaching SourceBuffer");
  }
  if (is_video)
    video_buffer_ = ret;
  else
    audio_buffer_ = ret;
  return ret;
}

ExceptionOr<void> MediaSource::EndOfStream(optional<std::string> error) {
  if (ready_state != MediaSourceReadyState::OPEN) {
    return JsError::DOMException(
        InvalidStateError,
        R"(Cannot call endOfStream() unless MediaSource is "open".)");
  }
  if ((video_buffer_ && video_buffer_->updating) ||
      (audio_buffer_ && audio_buffer_->updating)) {
    return JsError::DOMException(
        InvalidStateError,
        "Cannot call endOfStream() when a SourceBuffer is updating.");
  }
  if (error.has_value()) {
    return JsError::DOMException(
        NotSupportedError,
        "Calling endOfStream() with an argument is not supported.");
  }

  ready_state = MediaSourceReadyState::ENDED;
  ScheduleEvent<events::Event>(EventType::SourceEnded);

  player_->MseEndOfStream();
  return {};
}

double MediaSource::GetDuration() const {
  return player_ ? player_->Duration() : NAN;
}

ExceptionOr<void> MediaSource::SetDuration(double duration) {
  if (std::isnan(duration))
    return JsError::TypeError("Cannot set duration to NaN.");
  if (ready_state != MediaSourceReadyState::OPEN) {
    return JsError::DOMException(
        InvalidStateError,
        R"(Cannot change duration unless MediaSource is "open".)");
  }
  if ((video_buffer_ && video_buffer_->updating) ||
      (audio_buffer_ && audio_buffer_->updating)) {
    return JsError::DOMException(
        InvalidStateError,
        "Cannot change duration when a SourceBuffer is updating.");
  }

  DCHECK(player_);
  player_->SetDuration(duration);
  return {};
}

void MediaSource::OpenMediaSource(RefPtr<HTMLVideoElement> video,
                                  media::MediaPlayer* player) {
  DCHECK(ready_state == MediaSourceReadyState::CLOSED)
      << "MediaSource already attached to a <video> element.";
  ready_state = MediaSourceReadyState::OPEN;
  video_ = video;
  player_ = player;
  ScheduleEvent<events::Event>(EventType::SourceOpen);
}

void MediaSource::CloseMediaSource() {
  DCHECK(ready_state != MediaSourceReadyState::CLOSED)
      << "MediaSource not attached to a <video> element.";

  ready_state = MediaSourceReadyState::CLOSED;
  video_ = nullptr;
  player_ = nullptr;

  if (video_buffer_) {
    video_buffer_->Detach();
    video_buffer_.reset();
  }
  if (audio_buffer_) {
    audio_buffer_->Detach();
    audio_buffer_.reset();
  }

  ScheduleEvent<events::Event>(EventType::SourceClose);
}

void MediaSource::OnLoadedMetaData(double duration) {
  bool raise;
  if (got_loaded_metadata_) {
    // We only get this event once per buffer; so if this is called a second
    // time, we must have two buffers.
    raise = true;
  } else {
    // Raise if we only have one buffer.
    raise = video_buffer_.empty() != audio_buffer_.empty();
  }
  if (raise && player_)
    player_->LoadedMetaData(duration);
  got_loaded_metadata_ = true;
}

void MediaSource::OnEncrypted(eme::MediaKeyInitDataType type,
                              const uint8_t* data, size_t size) {
  if (video_) {
    video_->ScheduleEvent<events::MediaEncryptedEvent>(
        EventType::Encrypted, type, ByteBuffer(data, size));
  }
}


MediaSourceFactory::MediaSourceFactory() {
  AddListenerField(EventType::SourceOpen, &MediaSource::on_source_open);
  AddListenerField(EventType::SourceEnded, &MediaSource::on_source_ended);
  AddListenerField(EventType::SourceClose, &MediaSource::on_source_close);

  AddReadOnlyProperty("readyState", &MediaSource::ready_state);

  AddGenericProperty("duration", &MediaSource::GetDuration,
                     &MediaSource::SetDuration);

  AddMemberFunction("addSourceBuffer", &MediaSource::AddSourceBuffer);
  AddMemberFunction("endOfStream", &MediaSource::EndOfStream);

  AddStaticFunction("isTypeSupported", &MediaSource::IsTypeSupported);

  NotImplemented("activeSourceBuffers");
  NotImplemented("clearLiveSeekableRange");
  NotImplemented("removeSourceBuffer");
  NotImplemented("setLiveSeekableRange");
  NotImplemented("sourceBuffers");
}

}  // namespace mse
}  // namespace js
}  // namespace shaka
