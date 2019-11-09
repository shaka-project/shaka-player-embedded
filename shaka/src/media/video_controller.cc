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

#include "src/media/video_controller.h"

#include <glog/logging.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "src/core/js_manager_impl.h"
#include "src/media/audio_renderer.h"
#include "src/media/media_utils.h"
#include "src/media/video_renderer.h"
#include "src/util/clock.h"

namespace shaka {
namespace media {

namespace {

std::string FormatSize(const FrameBuffer* buffer) {
  const char* kSuffixes[] = {"", " KB", " MB", " GB", " TB"};
  size_t size = buffer->EstimateSize();
  for (const char* suffix : kSuffixes) {
    if (size < 3 * 1024)
      return std::to_string(size) + suffix;
    size /= 1024;
  }
  LOG(FATAL) << "Size too large to print.";
}

std::string FormatBuffered(const FrameBuffer* buffer) {
  std::string ret;
  for (auto& range : buffer->GetBufferedRanges()) {
    if (!ret.empty())
      ret += ", ";
    ret += util::StringPrintf("%.2f - %.2f", range.start, range.end);
  }
  return "[" + ret + "]";
}

Renderer* CreateRenderer(SourceType source, std::function<double()> get_time,
                         std::function<double()> get_playback_rate,
                         Stream* stream) {
  switch (source) {
    case SourceType::Audio:
      return new AudioRenderer(std::move(get_time),
                               std::move(get_playback_rate), stream);
    case SourceType::Video:
      return new VideoRenderer(std::move(get_time), stream);
    default:
      LOG(FATAL) << "Unknown source type: " << source;
  }
}

/**
 * A task that invokes the encrypted init data callback on the main thread.
 * We can't use PlainCallbackTask since that uses std::bind, which requires
 * the arguments to be copyable, which ByteBuffer isn't.
 */
class EncryptedInitDataTask : public memory::Traceable {
 public:
  EncryptedInitDataTask(
      std::function<void(eme::MediaKeyInitDataType, ByteBuffer&)> cb,
      eme::MediaKeyInitDataType type, ByteBuffer buffer)
      : cb_(cb), type_(type), buffer_(std::move(buffer)) {}

  void Trace(memory::HeapTracer* tracer) const override {
    // This shouldn't really be needed since there isn't a JavaScript buffer
    // backing it; but do it just to be safe.
    tracer->Trace(&buffer_);
  }

  void operator()() {
    cb_(type_, buffer_);
  }

 private:
  const std::function<void(eme::MediaKeyInitDataType, ByteBuffer&)> cb_;
  const eme::MediaKeyInitDataType type_;
  ByteBuffer buffer_;
};

}  // namespace

VideoController::VideoController(
    std::function<void(SourceType, Status)> on_error,
    std::function<void()> on_waiting_for_key,
    std::function<void(eme::MediaKeyInitDataType, ByteBuffer&)>
        on_encrypted_init_data,
    std::function<void(MediaReadyState)> on_ready_state_changed,
    std::function<void(PipelineStatus)> on_pipeline_changed)
    : mutex_("VideoController"),
      on_error_(std::move(on_error)),
      on_waiting_for_key_(std::move(on_waiting_for_key)),
      on_encrypted_init_data_(std::move(on_encrypted_init_data)),
      pipeline_(MainThreadCallback(std::move(on_pipeline_changed)),
                std::bind(&VideoController::OnSeek, this),
                &util::Clock::Instance),
      monitor_(std::bind(&VideoController::GetBufferedRanges, this,
                         SourceType::Unknown),
               std::bind(&VideoController::GetDecodedRanges, this),
               MainThreadCallback(std::move(on_ready_state_changed)),
               &util::Clock::Instance, &pipeline_),
      cdm_(nullptr) {
  Reset();
}

VideoController::~VideoController() {
  util::shared_lock<SharedMutex> lock(mutex_);
  for (const auto& source : sources_) {
    source.second->demuxer.Stop();
    source.second->decoder.Stop();
  }
  monitor_.Stop();
}

void VideoController::SetVolume(double volume) {
  std::unique_lock<SharedMutex> lock(mutex_);
  volume_ = volume;
  Source* source = GetSource(SourceType::Audio);
  if (source && source->renderer)
    static_cast<AudioRenderer*>(source->renderer.get())->SetVolume(volume);
}

Frame VideoController::DrawFrame(double* delay) {
  std::unique_lock<SharedMutex> lock(mutex_);
  Source* source = GetSource(SourceType::Video);
  if (!source || !source->renderer)
    return Frame();

  int dropped_frame_count = 0;
  bool is_new_frame = false;
  Frame ret =
      source->renderer->DrawFrame(&dropped_frame_count, &is_new_frame, delay);
  quality_info_.droppedVideoFrames += dropped_frame_count;
  quality_info_.totalVideoFrames += dropped_frame_count;
  if (is_new_frame)
    quality_info_.totalVideoFrames++;

#ifndef NDEBUG
  static uint64_t last_print_time = 0;
  static uint64_t last_dropped_frame_count = 0;
  const uint64_t cur_time = util::Clock::Instance.GetMonotonicTime();
  if (quality_info_.droppedVideoFrames != last_dropped_frame_count &&
      cur_time - last_print_time > 250) {
    const int delta_frames =
        quality_info_.droppedVideoFrames - last_dropped_frame_count;
    LOG(INFO) << "Dropped " << delta_frames << " frames.";
    last_print_time = cur_time;
    last_dropped_frame_count = quality_info_.droppedVideoFrames;
  }
#endif

  return ret;
}

void VideoController::SetCdm(eme::Implementation* cdm) {
  std::unique_lock<SharedMutex> lock(mutex_);
  cdm_ = cdm;
  for (auto& pair : sources_) {
    pair.second->decoder.SetCdm(cdm);
  }
}


Status VideoController::AddSource(const std::string& mime_type,
                                  SourceType* source_type) {
  std::unique_lock<SharedMutex> lock(mutex_);
  using namespace std::placeholders;  // NOLINT
  std::string container;
  std::string codec;
  if (!ParseMimeAndCheckSupported(mime_type, source_type, &container, &codec))
    return Status::NotSupported;
  if (sources_.count(*source_type) != 0)
    return Status::NotAllowed;

  std::unique_ptr<Source> source(new Source(
      *source_type, &pipeline_, container, codec,
      MainThreadCallback(on_waiting_for_key_),
      std::bind(&VideoController::OnEncryptedInitData, this, _1, _2, _3),
      std::bind(&PipelineManager::GetCurrentTime, &pipeline_),
      std::bind(&VideoController::GetPlaybackRate, this),
      std::bind(&VideoController::OnError, this, *source_type, _1),
      std::bind(&VideoController::OnLoadMeta, this, *source_type)));
  if (source->renderer) {
    if (*source_type == SourceType::Audio)
      static_cast<AudioRenderer*>(source->renderer.get())->SetVolume(volume_);
  }
  source->decoder.SetCdm(cdm_);
  sources_.emplace(*source_type, std::move(source));
  return Status::Success;
}

bool VideoController::AppendData(SourceType type, double timestamp_offset,
                                 double window_start, double window_end,
                                 const uint8_t* data, size_t data_size,
                                 std::function<void(Status)> on_complete) {
  util::shared_lock<SharedMutex> lock(mutex_);
  Source* source = GetSource(type);
  if (!source) {
    LOG(ERROR) << "Missing media source for " << type;
    return false;
  }

  source->demuxer.AppendData(timestamp_offset, window_start, window_end, data,
                             data_size, std::move(on_complete));
  return true;
}

bool VideoController::Remove(SourceType type, double start, double end) {
  util::shared_lock<SharedMutex> lock(mutex_);
  Source* source = GetSource(type);
  if (!source) {
    LOG(ERROR) << "Missing media source for " << type;
    return false;
  }

  source->stream.GetDemuxedFrames()->Remove(start, end);
  return true;
}

void VideoController::EndOfStream() {
  double duration = 0;
  {
    util::shared_lock<SharedMutex> lock(mutex_);
    for (auto& pair : sources_) {
      auto buffered = pair.second->stream.GetBufferedRanges();
      // Use the maximum duration of any stream as the total media duration.
      // See: https://w3c.github.io/media-source/#end-of-stream-algorithm
      if (!buffered.empty())
        duration = std::max(duration, buffered.back().end);
    }
  }

  pipeline_.SetDuration(duration);
}

BufferedRanges VideoController::GetBufferedRanges(SourceType type) const {
  util::shared_lock<SharedMutex> lock(mutex_);
  if (type == SourceType::Unknown) {
    std::vector<BufferedRanges> sources;
    sources.reserve(sources_.size());
    for (auto& pair : sources_)
      sources.push_back(pair.second->stream.GetBufferedRanges());
    return IntersectionOfBufferedRanges(sources);
  }

  const Source* source = GetSource(type);
  return source ? source->stream.GetBufferedRanges() : BufferedRanges();
}

void VideoController::Reset() {
  {
    util::shared_lock<SharedMutex> shared(mutex_);
    for (auto& source : sources_) {
      source.second->demuxer.Stop();
      source.second->decoder.Stop();
    }
  }

  std::unique_lock<SharedMutex> unique(mutex_);
  sources_.clear();
  cdm_ = nullptr;

  quality_info_.creationTime = NAN;
  quality_info_.totalVideoFrames = 0;
  quality_info_.droppedVideoFrames = 0;
  quality_info_.corruptedVideoFrames = 0;
}

void VideoController::DebugDumpStats() const {
  util::shared_lock<SharedMutex> lock(mutex_);

  printf("Video Stats:\n");
  printf("  Pipeline Status: %s\n",
         to_string(pipeline_.GetPipelineStatus()).c_str());
  printf("  Current Time: %.2f\n", pipeline_.GetCurrentTime());
  printf("  Duration: %.2f\n", pipeline_.GetDuration());
  printf("  Playback Rate: %.2f\n", pipeline_.GetPlaybackRate());
  for (auto& pair : sources_) {
    printf("  Buffer (%s):\n", to_string(pair.first).c_str());
    printf("    Demuxed (%s): %s\n",
           FormatSize(pair.second->stream.GetDemuxedFrames()).c_str(),
           FormatBuffered(pair.second->stream.GetDemuxedFrames()).c_str());
    printf("    Decoded (%s): %s\n",
           FormatSize(pair.second->stream.GetDecodedFrames()).c_str(),
           FormatBuffered(pair.second->stream.GetDecodedFrames()).c_str());
  }
}

void VideoController::OnSeek() {
  util::shared_lock<SharedMutex> lock(mutex_);
  for (auto& pair : sources_) {
    // No need to reset |processor| since that is done by the decoder.
    pair.second->decoder.OnSeek();
    if (pair.second->renderer)
      pair.second->renderer->OnSeek();
  }
}

void VideoController::OnLoadMeta(SourceType type) {
  bool done = true;
  double duration;
  {
    util::shared_lock<SharedMutex> lock(mutex_);
    DCHECK_EQ(1u, sources_.count(type));
    DCHECK(!sources_.at(type)->ready);
    sources_.at(type)->ready = true;
    duration = sources_.at(type)->processor.duration();

    for (auto& pair : sources_) {
      if (!pair.second->ready)
        done = false;
    }
  }
  if (std::isnan(pipeline_.GetDuration()))
    pipeline_.SetDuration(duration == 0 ? HUGE_VAL : duration);
  if (done)
    pipeline_.DoneInitializing();
}

void VideoController::OnError(SourceType type, Status error) {
  pipeline_.OnError();
  JsManagerImpl::Instance()->MainThread()->AddInternalTask(
      TaskPriority::Internal, "VideoController::OnError",
      PlainCallbackTask(std::bind(on_error_, type, error)));
}

void VideoController::OnEncryptedInitData(
    eme::MediaKeyInitDataType init_data_type, const uint8_t* data,
    size_t data_size) {
  JsManagerImpl::Instance()->MainThread()->AddInternalTask(
      TaskPriority::Internal, "VideoController::OnEncryptedInitData",
      EncryptedInitDataTask(on_encrypted_init_data_, init_data_type,
                            ByteBuffer(data, data_size)));
}

BufferedRanges VideoController::GetDecodedRanges() const {
  util::shared_lock<SharedMutex> lock(mutex_);
  std::vector<BufferedRanges> sources;
  sources.reserve(sources_.size());
  for (auto& pair : sources_) {
    sources.push_back(
        pair.second->stream.GetDecodedFrames()->GetBufferedRanges());
  }
  return IntersectionOfBufferedRanges(sources);
}

double VideoController::GetPlaybackRate() const {
  if (pipeline_.GetPipelineStatus() != PipelineStatus::Playing)
    return 0;

  return pipeline_.GetPlaybackRate();
}


VideoController::Source::Source(
    SourceType source_type, PipelineManager* pipeline,
    const std::string& container, const std::string& codecs,
    std::function<void()> on_waiting_for_key,
    std::function<void(eme::MediaKeyInitDataType, const uint8_t*, size_t)>
        on_encrypted_init_data,
    std::function<double()> get_time, std::function<double()> get_playback_rate,
    std::function<void(Status)> on_error, std::function<void()> on_load_meta)
    : processor(container, codecs, std::move(on_encrypted_init_data)),
      decoder(get_time, std::bind(&VideoController::Source::OnSeekDone, this),
              std::move(on_waiting_for_key), std::move(on_error), &processor,
              pipeline, &stream),
      demuxer(std::move(on_load_meta), &processor, &stream),
      renderer(CreateRenderer(source_type, get_time,
                              std::move(get_playback_rate), &stream)),
      ready(false) {}

VideoController::Source::~Source() {}

void VideoController::Source::OnSeekDone() {
  if (renderer)
    renderer->OnSeekDone();
}

}  // namespace media
}  // namespace shaka
