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
#include "src/media/ffmpeg/ffmpeg_decoder.h"
#include "src/media/media_utils.h"
#include "src/util/clock.h"

namespace shaka {
namespace media {

namespace {

std::string FormatSize(const StreamBase& buffer) {
  const char* kSuffixes[] = {"", " KB", " MB", " GB", " TB"};
  size_t size = buffer.EstimateSize();
  for (const char* suffix : kSuffixes) {
    if (size < 3 * 1024)
      return std::to_string(size) + suffix;
    size /= 1024;
  }
  LOG(FATAL) << "Size too large to print.";
}

std::string FormatBuffered(const StreamBase& buffer) {
  std::string ret;
  for (auto& range : buffer.GetBufferedRanges()) {
    if (!ret.empty())
      ret += ", ";
    ret += util::StringPrintf("%.2f - %.2f", range.start, range.end);
  }
  return "[" + ret + "]";
}

/**
 * A task that invokes the encrypted init data callback on the main thread.
 * We can't use PlainCallbackTask since that uses std::bind, which requires
 * the arguments to be copyable, which ByteBuffer isn't.
 */
class EncryptedInitDataTask : public memory::Traceable {
 public:
  EncryptedInitDataTask(
      std::function<void(eme::MediaKeyInitDataType, ByteBuffer)> cb,
      eme::MediaKeyInitDataType type, ByteBuffer buffer)
      : cb_(cb), type_(type), buffer_(std::move(buffer)) {}

  void Trace(memory::HeapTracer* tracer) const override {
    // This shouldn't really be needed since there isn't a JavaScript buffer
    // backing it; but do it just to be safe.
    tracer->Trace(&buffer_);
  }

  void operator()() {
    cb_(type_, std::move(buffer_));
  }

 private:
  const std::function<void(eme::MediaKeyInitDataType, ByteBuffer)> cb_;
  const eme::MediaKeyInitDataType type_;
  ByteBuffer buffer_;
};

/**
 * A fake MediaPlayer implementation that forwards some calls to
 * VideoController.  This exists temporarily as we migrate the Renderers to the
 * new API.
 */
class FakeMediaPlayer : public MediaPlayer {
 public:
  explicit FakeMediaPlayer(VideoController* controller)
      : controller_(controller) {}

  MediaCapabilitiesInfo DecodingInfo(
      const MediaDecodingConfiguration& config) const override {
    LOG(FATAL) << "Not implemented";
  }
  VideoPlaybackQualityNew VideoPlaybackQuality() const override {
    LOG(FATAL) << "Not implemented";
  }
  void AddClient(Client* client) override {}
  void RemoveClient(Client* client) override {}
  std::vector<BufferedRange> GetBuffered() const override {
    LOG(FATAL) << "Not implemented";
  }
  VideoReadyState ReadyState() const override {
    LOG(FATAL) << "Not implemented";
  }
  VideoPlaybackState PlaybackState() const override {
    const auto status = controller_->GetPipelineManager()->GetPipelineStatus();
    switch (status) {
      case PipelineStatus::Initializing:
      case PipelineStatus::Errored:
        return VideoPlaybackState::Initializing;
      case PipelineStatus::Playing:
        return VideoPlaybackState::Playing;
      case PipelineStatus::Paused:
        return VideoPlaybackState::Paused;
      case PipelineStatus::SeekingPlay:
      case PipelineStatus::SeekingPause:
        return VideoPlaybackState::Seeking;
      case PipelineStatus::Stalled:
        return VideoPlaybackState::Buffering;
      case PipelineStatus::Ended:
        return VideoPlaybackState::Ended;
    }
  }

  bool SetVideoFillMode(VideoFillMode mode) override {
    LOG(FATAL) << "Not implemented";
  }
  uint32_t Width() const override {
    LOG(FATAL) << "Not implemented";
  }
  uint32_t Height() const override {
    LOG(FATAL) << "Not implemented";
  }
  double Volume() const override {
    LOG(FATAL) << "Not implemented";
  }
  void SetVolume(double volume) override {
    LOG(FATAL) << "Not implemented";
  }
  bool Muted() const override {
    LOG(FATAL) << "Not implemented";
  }
  void SetMuted(bool muted) override {
    LOG(FATAL) << "Not implemented";
  }

  void Play() override {
    LOG(FATAL) << "Not implemented";
  }
  void Pause() override {
    LOG(FATAL) << "Not implemented";
  }
  double CurrentTime() const override {
    return controller_->GetPipelineManager()->GetCurrentTime();
  }
  void SetCurrentTime(double time) override {
    LOG(FATAL) << "Not implemented";
  }
  double Duration() const override {
    return controller_->GetPipelineManager()->GetDuration();
  }
  void SetDuration(double duration) override {
    LOG(FATAL) << "Not implemented";
  }
  double PlaybackRate() const override {
    return controller_->GetPipelineManager()->GetPlaybackRate();
  }
  void SetPlaybackRate(double rate) override {
    LOG(FATAL) << "Not implemented";
  }

  bool AttachSource(const std::string& src) override {
    return false;
  }
  bool AttachMse() override {
    return false;
  }
  bool AddMseBuffer(const std::string& mime, bool is_video,
                    const ElementaryStream* stream) override {
    return false;
  }
  void LoadedMetaData(double duration) override {}
  void MseEndOfStream() override {}
  bool SetEmeImplementation(const std::string& key_system,
                            eme::Implementation* implementation) override {
    return false;
  }
  void Detach() override {}

 private:
  VideoController* controller_;
};

}  // namespace

VideoController::VideoController(
    std::function<void(SourceType, Status)> on_error,
    std::function<void()> on_waiting_for_key,
    std::function<void(eme::MediaKeyInitDataType, ByteBuffer)>
        on_encrypted_init_data,
    std::function<void(VideoReadyState)> on_ready_state_changed,
    std::function<void(PipelineStatus)> on_pipeline_changed)
    : mutex_("VideoController"),
      fake_media_player_(new FakeMediaPlayer(this)),
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
      cdm_(nullptr),
      video_renderer_(nullptr),
      audio_renderer_(nullptr),
      init_count_(0) {
  Reset();
}

VideoController::~VideoController() {
  util::shared_lock<SharedMutex> lock(mutex_);
  for (const auto& source : sources_) {
    source.second->demuxer.Stop();
    if (source.second->renderer) {
      source.second->renderer->Detach();
      source.second->renderer->SetPlayer(nullptr);
    }
  }
  monitor_.Stop();
}

void VideoController::SetCdm(eme::Implementation* cdm) {
  std::unique_lock<SharedMutex> lock(mutex_);
  cdm_ = cdm;
  for (auto& pair : sources_) {
    pair.second->decoder_thread.SetCdm(cdm);
  }
}

void VideoController::SetRenderers(VideoRenderer* video_renderer,
                                   AudioRenderer* audio_renderer) {
  std::unique_lock<SharedMutex> lock(mutex_);
  video_renderer_ = video_renderer;
  audio_renderer_ = audio_renderer;
  video_renderer->SetPlayer(fake_media_player_.get());
  audio_renderer->SetPlayer(fake_media_player_.get());

  Source* video = GetSource(SourceType::Video);
  if (video) {
    video->renderer = video_renderer;
    video_renderer->Attach(&video->decoded_frames);
  }

  Source* audio = GetSource(SourceType::Audio);
  if (audio) {
    audio->renderer = audio_renderer;
    audio_renderer->Attach(&audio->decoded_frames);
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

  std::unique_ptr<Source> source(
      new Source(*source_type, &pipeline_, this, mime_type,
                 MainThreadCallback(on_waiting_for_key_),
                 std::bind(&PipelineManager::GetCurrentTime, &pipeline_),
                 std::bind(&VideoController::OnError, this, *source_type, _1)));
  source->decoder_thread.SetCdm(cdm_);
  if (*source_type == SourceType::Video && video_renderer_) {
    source->renderer = video_renderer_;
    video_renderer_->Attach(&source->decoded_frames);
  } else if (*source_type == SourceType::Audio && audio_renderer_) {
    source->renderer = audio_renderer_;
    audio_renderer_->Attach(&source->decoded_frames);
  }

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

  source->encoded_frames.Remove(start, end);
  return true;
}

void VideoController::EndOfStream() {
  double duration = 0;
  {
    util::shared_lock<SharedMutex> lock(mutex_);
    for (auto& pair : sources_) {
      auto buffered = pair.second->encoded_frames.GetBufferedRanges();
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
      sources.push_back(pair.second->encoded_frames.GetBufferedRanges());
    return IntersectionOfBufferedRanges(sources);
  }

  const Source* source = GetSource(type);
  return source ? source->encoded_frames.GetBufferedRanges() : BufferedRanges();
}

void VideoController::Reset() {
  {
    util::shared_lock<SharedMutex> shared(mutex_);
    for (auto& source : sources_) {
      source.second->demuxer.Stop();
      source.second->decoder_thread.Detach();
      if (source.second->renderer) {
        source.second->renderer->Detach();
        source.second->renderer->SetPlayer(nullptr);
      }
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
           FormatSize(pair.second->encoded_frames).c_str(),
           FormatBuffered(pair.second->encoded_frames).c_str());
    printf("    Decoded (%s): %s\n",
           FormatSize(pair.second->decoded_frames).c_str(),
           FormatBuffered(pair.second->decoded_frames).c_str());
  }
}

void VideoController::OnSeek() {
  util::shared_lock<SharedMutex> lock(mutex_);
  for (auto& pair : sources_) {
    pair.second->decoder_thread.OnSeek();
    if (pair.second->renderer)
      pair.second->renderer->OnSeek();
  }
}

void VideoController::OnLoadedMetaData(double duration) {
  bool done;
  {
    util::shared_lock<SharedMutex> lock(mutex_);
    DCHECK_LT(init_count_, sources_.size());
    init_count_++;
    done = init_count_ == sources_.size();
  }

  if (std::isnan(pipeline_.GetDuration()))
    pipeline_.SetDuration(duration);
  if (done)
    pipeline_.DoneInitializing();
}

void VideoController::OnError(SourceType type, Status error) {
  pipeline_.OnError();
  JsManagerImpl::Instance()->MainThread()->AddInternalTask(
      TaskPriority::Internal, "VideoController::OnError",
      PlainCallbackTask(std::bind(on_error_, type, error)));
}

void VideoController::OnEncrypted(eme::MediaKeyInitDataType init_data_type,
                                  const uint8_t* data, size_t data_size) {
  JsManagerImpl::Instance()->MainThread()->AddInternalTask(
      TaskPriority::Internal, "VideoController::OnEncrypted",
      EncryptedInitDataTask(on_encrypted_init_data_, init_data_type,
                            ByteBuffer(data, data_size)));
}

BufferedRanges VideoController::GetDecodedRanges() const {
  util::shared_lock<SharedMutex> lock(mutex_);
  std::vector<BufferedRanges> sources;
  sources.reserve(sources_.size());
  for (auto& pair : sources_) {
    sources.push_back(pair.second->decoded_frames.GetBufferedRanges());
  }
  return IntersectionOfBufferedRanges(sources);
}

double VideoController::GetPlaybackRate() const {
  if (pipeline_.GetPipelineStatus() != PipelineStatus::Playing)
    return 0;

  return pipeline_.GetPlaybackRate();
}


VideoController::Source::Source(SourceType source_type,
                                PipelineManager* pipeline,
                                Demuxer::Client* demuxer_client,
                                const std::string& mime,
                                std::function<void()> on_waiting_for_key,
                                std::function<double()> get_time,
                                std::function<void(Status)> on_error)
    : decoder(new ffmpeg::FFmpegDecoder),
      decoder_thread(this, &decoded_frames),
      demuxer(mime, demuxer_client, &encoded_frames),
      renderer(nullptr),
      ready(false),
      pipeline(pipeline),
      get_time(get_time),
      on_waiting_for_key(on_waiting_for_key),
      on_error(on_error) {
  decoder_thread.SetDecoder(decoder.get());
  decoder_thread.Attach(&encoded_frames);
}

VideoController::Source::~Source() {}

double VideoController::Source::CurrentTime() const {
  return get_time();
}

double VideoController::Source::Duration() const {
  return pipeline->GetDuration();
}

void VideoController::Source::OnWaitingForKey() {
  on_waiting_for_key();
}

void VideoController::Source::OnSeekDone() {}

void VideoController::Source::OnError() {
  on_error(Status::UnknownError);
}

}  // namespace media
}  // namespace shaka
