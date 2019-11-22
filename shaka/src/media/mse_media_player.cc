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

#include "src/media/mse_media_player.h"

#include <algorithm>
#include <functional>

#include "src/media/ffmpeg/ffmpeg_decoder.h"
#include "src/media/media_utils.h"

namespace shaka {
namespace media {

MseMediaPlayer::MseMediaPlayer(VideoRenderer* video_renderer,
                               AudioRenderer* audio_renderer)
    : mutex_("MseMediaPlayer"),
      pipeline_manager_(std::bind(&MseMediaPlayer::OnStatusChanged, this,
                                  std::placeholders::_1),
                        std::bind(&MseMediaPlayer::OnSeek, this),
                        &util::Clock::Instance),
      pipeline_monitor_(std::bind(&MseMediaPlayer::GetBuffered, this),
                        std::bind(&MseMediaPlayer::GetBuffered, this),
                        std::bind(&MseMediaPlayer::ReadyStateChanged, this,
                                  std::placeholders::_1),
                        &util::Clock::Instance, &pipeline_manager_),
      old_status_(PipelineStatus::Initializing),
      ready_state_(VideoReadyState::NotAttached),
      video_(this),
      audio_(this),
      video_renderer_(video_renderer),
      audio_renderer_(audio_renderer) {
  video_renderer_->SetPlayer(this);
  audio_renderer_->SetPlayer(this);
}

MseMediaPlayer::~MseMediaPlayer() {
  video_renderer_->SetPlayer(nullptr);
  audio_renderer_->SetPlayer(nullptr);
}

void MseMediaPlayer::SetDecoders(Decoder* video_decoder,
                                 Decoder* audio_decoder) {
  std::unique_lock<SharedMutex> lock(mutex_);
  video_.SetDecoder(video_decoder);
  audio_.SetDecoder(audio_decoder);
}

MediaCapabilitiesInfo MseMediaPlayer::DecodingInfo(
    const MediaDecodingConfiguration& config) const {
  if (config.type != MediaDecodingType::MediaSource ||
      (config.video.content_type.empty() &&
       config.audio.content_type.empty())) {
    return MediaCapabilitiesInfo();
  }

  util::shared_lock<SharedMutex> lock(mutex_);
  MediaCapabilitiesInfo ret;
  ret.supported = ret.power_efficient = ret.smooth = true;
  if (!config.video.content_type.empty()) {
    auto* decoder = video_.GetDecoder();
    if (!decoder)
      return MediaCapabilitiesInfo();

    auto new_config = config;
    new_config.audio.content_type = "";
    ret = ret & decoder->DecodingInfo(new_config);
  }
  if (!config.audio.content_type.empty()) {
    auto* decoder = audio_.GetDecoder();
    if (!decoder)
      return MediaCapabilitiesInfo();

    auto new_config = config;
    new_config.video.content_type = "";
    ret = ret & decoder->DecodingInfo(new_config);
  }

  return ret;
}

VideoPlaybackQualityNew MseMediaPlayer::VideoPlaybackQuality() const {
  return video_renderer_->VideoPlaybackQuality();
}

void MseMediaPlayer::AddClient(MediaPlayer::Client* client) {
  std::unique_lock<SharedMutex> lock(mutex_);
  clients_.insert(client);
}

void MseMediaPlayer::RemoveClient(MediaPlayer::Client* client) {
  std::unique_lock<SharedMutex> lock(mutex_);
  clients_.erase(client);
}

std::vector<BufferedRange> MseMediaPlayer::GetBuffered() const {
  util::shared_lock<SharedMutex> lock(mutex_);
  std::vector<std::vector<BufferedRange>> ranges;
  for (auto* ptr : {&video_, &audio_}) {
    if (ptr->IsAttached())
      ranges.emplace_back(ptr->GetBuffered());
  }
  return IntersectionOfBufferedRanges(ranges);
}

VideoReadyState MseMediaPlayer::ReadyState() const {
  util::shared_lock<SharedMutex> lock(mutex_);
  return ready_state_;
}

VideoPlaybackState MseMediaPlayer::PlaybackState() const {
  const auto status = pipeline_manager_.GetPipelineStatus();
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

std::vector<std::shared_ptr<TextTrack>> MseMediaPlayer::TextTracks() {
  // TextTrack usage should be done through DefaultMediaPlayer.
  LOG(FATAL) << "Not implemented";
}

std::vector<std::shared_ptr<const TextTrack>> MseMediaPlayer::TextTracks()
    const {
  // TextTrack usage should be done through DefaultMediaPlayer.
  LOG(FATAL) << "Not implemented";
}

std::shared_ptr<TextTrack> MseMediaPlayer::AddTextTrack(
    TextTrackKind kind, const std::string& label, const std::string& language) {
  // TextTrack usage should be done through DefaultMediaPlayer.
  LOG(FATAL) << "Not implemented";
}


bool MseMediaPlayer::SetVideoFillMode(VideoFillMode mode) {
  return video_renderer_->SetVideoFillMode(mode);
}

uint32_t MseMediaPlayer::Height() const {
  double time = pipeline_manager_.GetCurrentTime();

  util::shared_lock<SharedMutex> lock(mutex_);
  auto frame = video_.GetFrame(time);
  return frame ? frame->height : 0;
}

uint32_t MseMediaPlayer::Width() const {
  double time = pipeline_manager_.GetCurrentTime();

  util::shared_lock<SharedMutex> lock(mutex_);
  auto frame = video_.GetFrame(time);
  return frame ? frame->width : 0;
}

double MseMediaPlayer::Volume() const {
  return audio_renderer_->Volume();
}

void MseMediaPlayer::SetVolume(double volume) {
  audio_renderer_->SetVolume(volume);
}

bool MseMediaPlayer::Muted() const {
  return audio_renderer_->Muted();
}

void MseMediaPlayer::SetMuted(bool muted) {
  audio_renderer_->SetMuted(muted);
}

void MseMediaPlayer::Play() {
  pipeline_manager_.Play();
}

void MseMediaPlayer::Pause() {
  pipeline_manager_.Pause();
}

double MseMediaPlayer::CurrentTime() const {
  return pipeline_manager_.GetCurrentTime();
}

void MseMediaPlayer::SetCurrentTime(double time) {
  pipeline_manager_.SetCurrentTime(time);
}

double MseMediaPlayer::Duration() const {
  return pipeline_manager_.GetDuration();
}

void MseMediaPlayer::SetDuration(double duration) {
  pipeline_manager_.SetDuration(duration);
}

double MseMediaPlayer::PlaybackRate() const {
  return pipeline_manager_.GetPlaybackRate();
}

void MseMediaPlayer::SetPlaybackRate(double rate) {
  pipeline_manager_.SetPlaybackRate(rate);
}


bool MseMediaPlayer::AttachSource(const std::string& src) {
  return false;
}

bool MseMediaPlayer::AttachMse() {
  {
    std::unique_lock<SharedMutex> lock(mutex_);
    old_status_ = PipelineStatus::Initializing;
    ready_state_ = VideoReadyState::HaveNothing;
  }

  pipeline_manager_.Reset();
  pipeline_monitor_.Start();
  return true;
}

bool MseMediaPlayer::AddMseBuffer(const std::string& mime, bool is_video,
                                  const ElementaryStream* stream) {
  {
    std::unique_lock<SharedMutex> lock(mutex_);
    if (is_video)
      video_.Attach(stream);
    else
      audio_.Attach(stream);
  }

  // Avoid holding the lock for interacting with the Renderers.
  if (is_video)
    video_renderer_->Attach(video_.GetDecodedStream());
  else
    audio_renderer_->Attach(audio_.GetDecodedStream());
  return true;
}

void MseMediaPlayer::LoadedMetaData(double duration) {
  if (std::isfinite(duration) && !std::isfinite(Duration()))
    SetDuration(duration);
  pipeline_manager_.DoneInitializing();
}

void MseMediaPlayer::MseEndOfStream() {
  double duration = 0;
  {
    util::shared_lock<SharedMutex> lock(mutex_);
    // Use the maximum duration of any stream as the total media duration.
    // See: https://w3c.github.io/media-source/#end-of-stream-algorithm
    for (auto* ptr : {&video_, &audio_}) {
      if (ptr->IsAttached()) {
        auto buffered = ptr->GetBuffered();
        if (!buffered.empty())
          duration = std::max(duration, buffered.back().end);
      }
    }
  }
  pipeline_manager_.SetDuration(duration);
}

bool MseMediaPlayer::SetEmeImplementation(const std::string& /* key_system */,
                                          eme::Implementation* implementation) {
  std::unique_lock<SharedMutex> lock(mutex_);
  video_.SetCdm(implementation);
  audio_.SetCdm(implementation);
  return true;
}

void MseMediaPlayer::Detach() {
  // Avoid holding the lock for interacting with the Renderers.
  audio_renderer_->Detach();
  video_renderer_->Detach();
  pipeline_monitor_.Stop();

  std::unique_lock<SharedMutex> lock(mutex_);
  video_.Detach();
  audio_.Detach();
  ready_state_ = VideoReadyState::NotAttached;
}

void MseMediaPlayer::OnStatusChanged(PipelineStatus status) {
  VideoPlaybackState old_state = PlaybackState();
  PipelineStatus old_status;
  {
    std::unique_lock<SharedMutex> lock(mutex_);
    old_status = old_status_;
    old_status_ = status;
  }

  util::shared_lock<SharedMutex> lock(mutex_);

  if (status == old_status) {
    // If we get another seeking status change, we still fire the 'seeking'
    // event since the current time changed.
    if (status == media::PipelineStatus::SeekingPlay ||
        status == media::PipelineStatus::SeekingPause) {
      for (auto* client : clients_)
        client->OnSeeking();
    }
    return;
  }

  for (auto* client : clients_)
    client->OnPlaybackStateChanged(old_state, PlaybackState());
  switch (status) {
    case media::PipelineStatus::Initializing:
    case media::PipelineStatus::Playing:
      if (old_status == media::PipelineStatus::Paused) {
        for (auto* client : clients_)
          client->OnPlay();
      }
      break;
    case media::PipelineStatus::SeekingPlay:
    case media::PipelineStatus::SeekingPause:
      for (auto* client : clients_)
        client->OnSeeking();
      break;
    case media::PipelineStatus::Errored:
      for (auto* client : clients_)
        client->OnError("");
      break;
    case media::PipelineStatus::Paused:
    case media::PipelineStatus::Stalled:
    case media::PipelineStatus::Ended:
      break;
  }
}

void MseMediaPlayer::ReadyStateChanged(VideoReadyState state) {
  VideoReadyState old;
  {
    std::unique_lock<SharedMutex> lock(mutex_);
    old = ready_state_;
    ready_state_ = state;
  }

  util::shared_lock<SharedMutex> lock(mutex_);
  for (auto* client : clients_)
    client->OnReadyStateChanged(old, state);
}

void MseMediaPlayer::OnSeek() {
  // Avoid holding the lock for interacting with the Renderers.
  audio_renderer_->OnSeek();
  video_renderer_->OnSeek();

  std::unique_lock<SharedMutex> lock(mutex_);
  video_.OnSeek();
  audio_.OnSeek();
}

void MseMediaPlayer::OnError() {
  pipeline_manager_.OnError();
}

void MseMediaPlayer::OnWaitingForKey() {
  util::shared_lock<SharedMutex> lock(mutex_);
  for (auto* client : clients_)
    client->OnWaitingForKey();
}


MseMediaPlayer::Source::Source(MseMediaPlayer* player)
    : default_decoder_(new ffmpeg::FFmpegDecoder),
      decoder_thread_(player, &decoded_frames_),
      input_(nullptr),
      decoder_(nullptr) {
  decoder_thread_.SetDecoder(GetDecoder());
}

MseMediaPlayer::Source::~Source() {}

DecodedStream* MseMediaPlayer::Source::GetDecodedStream() {
  return &decoded_frames_;
}

Decoder* MseMediaPlayer::Source::GetDecoder() const {
  return decoder_ ? decoder_ : default_decoder_.get();
}

void MseMediaPlayer::Source::SetDecoder(Decoder* decoder) {
  decoder_ = decoder;
  decoder_thread_.SetDecoder(GetDecoder());
}

std::vector<BufferedRange> MseMediaPlayer::Source::GetBuffered() const {
  return input_ ? input_->GetBufferedRanges() : std::vector<BufferedRange>{};
}

std::shared_ptr<DecodedFrame> MseMediaPlayer::Source::GetFrame(
    double time) const {
  return decoded_frames_.GetFrame(time, FrameLocation::Near);
}

bool MseMediaPlayer::Source::IsAttached() const {
  return input_;
}

void MseMediaPlayer::Source::Attach(const ElementaryStream* stream) {
  DCHECK(!IsAttached());
  decoded_frames_.Clear();
  decoder_thread_.Attach(stream);
  input_ = stream;
}

void MseMediaPlayer::Source::Detach() {
  decoder_thread_.Detach();
  input_ = nullptr;
}

void MseMediaPlayer::Source::OnSeek() {
  decoder_thread_.OnSeek();
}

void MseMediaPlayer::Source::SetCdm(eme::Implementation* cdm) {
  decoder_thread_.SetCdm(cdm);
}

}  // namespace media
}  // namespace shaka