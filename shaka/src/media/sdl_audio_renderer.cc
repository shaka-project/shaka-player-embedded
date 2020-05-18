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

#include "shaka/media/sdl_audio_renderer.h"

#include <SDL2/SDL.h>

#include "shaka/utils.h"
#include "src/media/audio_renderer_common.h"
#include "src/util/utils.h"

namespace shaka {
namespace media {

namespace {

bool SDLFormatFromShaka(variant<PixelFormat, SampleFormat> format,
                        SDL_AudioFormat* result) {
  // Try to use the same format to avoid work by swresample.
  switch (get<SampleFormat>(format)) {
    case SampleFormat::PackedU8:
    case SampleFormat::PlanarU8:
      *result = AUDIO_U8;
      return true;
    case SampleFormat::PackedS16:
    case SampleFormat::PlanarS16:
      *result = AUDIO_S16SYS;
      return true;
    case SampleFormat::PackedS32:
    case SampleFormat::PlanarS32:
      *result = AUDIO_S32SYS;
      return true;
    case SampleFormat::PackedFloat:
    case SampleFormat::PlanarFloat:
      *result = AUDIO_F32SYS;
      return true;

    case SampleFormat::PackedDouble:
    case SampleFormat::PlanarDouble:
      LOG(DFATAL) << "SDL doesn't support double-precision audio.";
      return false;

    case SampleFormat::PackedS64:
    case SampleFormat::PlanarS64:
      LOG(DFATAL) << "SDL doesn't support 64-bit audio.";
      return false;

    default:
      LOG(DFATAL) << "Unknown audio sample format: " << format;
      return false;
  }
}

bool InitSdl() {
  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    SDL_SetMainReady();
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
      LOG(ERROR) << "Error initializing SDL: " << SDL_GetError();
      return false;
    }
  }
  return true;
}

}  // namespace

class SdlAudioRenderer::Impl : public AudioRendererCommon {
 public:
  explicit Impl(const std::string& device_name)
      : device_name_(device_name), audio_device_(0), volume_(0) {
    // Use "playback" mode on iOS.  This ensures the audio remains playing when
    // locked or muted.
    SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "playback");
  }

  ~Impl() override {
    Stop();
    ResetDevice();
  }

  void Detach() override {
    AudioRendererCommon::Detach();
    // We can only open a device once; so once we detach, we should close the
    // device so another renderer can run.
    ResetDevice();
  }

 private:
  bool InitDevice(std::shared_ptr<DecodedFrame> frame, double volume) override {
    if (!InitSdl())
      return false;
    ResetDevice();

    SDL_AudioSpec obtained_audio_spec;
    SDL_AudioSpec audio_spec;
    memset(&audio_spec, 0, sizeof(audio_spec));
    if (!SDLFormatFromShaka(frame->format, &audio_spec.format))
      return false;
    audio_spec.freq = frame->stream_info->sample_rate;
    audio_spec.channels = static_cast<Uint8>(frame->stream_info->channel_count);
    audio_spec.samples = static_cast<Uint16>(frame->sample_count);

    const char* device = device_name_.empty() ? nullptr : device_name_.c_str();
    audio_device_ =
        SDL_OpenAudioDevice(device, 0, &audio_spec, &obtained_audio_spec, 0);
    if (audio_device_ == 0) {
      LOG(DFATAL) << "Error opening audio device: " << SDL_GetError();
      return false;
    }

    format_ = obtained_audio_spec.format;
    volume_ = volume;
    return true;
  }

  bool AppendBuffer(const uint8_t* data, size_t size) override {
    std::vector<uint8_t> temp(size);
    SDL_MixAudioFormat(temp.data(), data, format_, size,
                       static_cast<int>(volume_ * SDL_MIX_MAXVOLUME));
    if (SDL_QueueAudio(audio_device_, temp.data(), size) != 0) {
      LOG(DFATAL) << "Error appending audio: " << SDL_GetError();
      return false;
    }
    return true;
  }

  void ClearBuffer() override {
    if (audio_device_ != 0)
      SDL_ClearQueuedAudio(audio_device_);
  }

  size_t GetBytesBuffered() const override {
    if (audio_device_ != 0)
      return SDL_GetQueuedAudioSize(audio_device_);
    else
      return 0;
  }

  void SetDeviceState(bool is_playing) override {
    if (audio_device_ != 0)
      SDL_PauseAudioDevice(audio_device_, is_playing ? 0 : 1);
  }

  void UpdateVolume(double volume) override {
    volume_ = volume;
  }

  void ResetDevice() {
    if (audio_device_ > 0) {
      SDL_CloseAudioDevice(audio_device_);
      audio_device_ = 0;
    }
  }

  const std::string device_name_;
  SDL_AudioDeviceID audio_device_;
  SDL_AudioFormat format_;
  double volume_;
};


SdlAudioRenderer::SdlAudioRenderer(const std::string& device_name)
    : impl_(new Impl(device_name)) {}
SdlAudioRenderer::~SdlAudioRenderer() {}

std::vector<std::string> SdlAudioRenderer::ListDevices() {
  if (!InitSdl())
    return {};

  std::vector<std::string> ret(SDL_GetNumAudioDevices(/* iscapture= */ 0));
  for (size_t i = 0; i < ret.size(); i++)
    ret[i] = SDL_GetAudioDeviceName(i, /* iscapture= */ 0);
  return ret;
}

void SdlAudioRenderer::SetPlayer(const MediaPlayer* player) {
  impl_->SetPlayer(player);
}
void SdlAudioRenderer::Attach(const DecodedStream* stream) {
  impl_->Attach(stream);
}
void SdlAudioRenderer::Detach() {
  impl_->Detach();
}

double SdlAudioRenderer::Volume() const {
  return impl_->Volume();
}
void SdlAudioRenderer::SetVolume(double volume) {
  impl_->SetVolume(volume);
}
bool SdlAudioRenderer::Muted() const {
  return impl_->Muted();
}
void SdlAudioRenderer::SetMuted(bool muted) {
  impl_->SetMuted(muted);
}

}  // namespace media
}  // namespace shaka
