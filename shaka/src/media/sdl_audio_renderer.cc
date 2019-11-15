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

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include "shaka/utils.h"
#include "src/debug/mutex.h"
#include "src/debug/thread.h"
#include "src/debug/thread_event.h"
#include "src/util/clock.h"
#include "src/util/macros.h"
#include "src/util/utils.h"

namespace shaka {
namespace media {

namespace {

/**
 * The maximum playback rate we will adjust audio for.  If the playback rate
 * is more than this, we will mute the audio.
 */
constexpr const double kMaxPlaybackRate = 4;

/**
 * The maximum delay, in seconds, between the frame time and the real time it
 * will be played before a seek happens.  This can happen when muted or if the
 * frames have gaps.  If the delay is too large, simulate a seek and start
 * playing frames based on the current real time.
 */
constexpr const double kMaxAudioDelay = 0.2;


SDL_AudioFormat SDLFormatFromShaka(variant<PixelFormat, SampleFormat> format) {
  // Try to use the same format to avoid work by swresample.
  switch (get<SampleFormat>(format)) {
    case SampleFormat::PackedU8:
    case SampleFormat::PlanarU8:
      return AUDIO_U8;
    case SampleFormat::PackedS16:
    case SampleFormat::PlanarS16:
      return AUDIO_S16SYS;
    case SampleFormat::PackedS32:
    case SampleFormat::PlanarS32:
      return AUDIO_S32SYS;
    case SampleFormat::PackedFloat:
    case SampleFormat::PlanarFloat:
      return AUDIO_F32SYS;

    case SampleFormat::PackedDouble:
    case SampleFormat::PlanarDouble: {
      LOG_ONCE(WARNING) << "SDL doesn't support double-precision audio "
                        << "formats, converting to floats.";
      return AUDIO_F32SYS;
    }
    case SampleFormat::PackedS64:
    case SampleFormat::PlanarS64: {
      LOG_ONCE(WARNING) << "SDL doesn't support 64-bit audio "
                        << "formats, converting to 32-bit.";
      return AUDIO_S32SYS;
    }

    default:
      LOG(DFATAL) << "Unknown audio sample format: "
                  << static_cast<int>(get<SampleFormat>(format));
      return AUDIO_S32SYS;
  }
}

AVSampleFormat FFmpegFormatFromSDL(int format) {
  // Note that AUDIO_*SYS is an alias for either AUDIO_*LSB or AUDIO_*MSB.
  switch (format) {
    case AUDIO_U8:
      return AV_SAMPLE_FMT_U8;
    case AUDIO_S16LSB:
    case AUDIO_S16MSB:
      if (format != AUDIO_S16SYS) {
        LOG(ERROR) << "swresample doesn't support non-native endian audio";
        return AV_SAMPLE_FMT_NONE;
      }
      return AV_SAMPLE_FMT_S16;
    case AUDIO_S32LSB:
    case AUDIO_S32MSB:
      if (format != AUDIO_S32SYS) {
        LOG(ERROR) << "swresample doesn't support non-native endian audio";
        return AV_SAMPLE_FMT_NONE;
      }
      return AV_SAMPLE_FMT_S32;
    case AUDIO_F32LSB:
    case AUDIO_F32MSB:
      if (format != AUDIO_F32SYS) {
        LOG(ERROR) << "swresample doesn't support non-native endian audio";
        return AV_SAMPLE_FMT_NONE;
      }
      return AV_SAMPLE_FMT_FLT;

    case AUDIO_S8:
      LOG(ERROR) << "swresample doesn't support signed 8-bit audio.";
      return AV_SAMPLE_FMT_NONE;
    case AUDIO_U16LSB:
    case AUDIO_U16MSB:
      LOG(ERROR) << "swresample doesn't support unsigned 16-bit audio";
      return AV_SAMPLE_FMT_NONE;

    default:
      LOG(DFATAL) << "Unknown audio sample format: " << format;
      return AV_SAMPLE_FMT_S32;
  }
}

int GetFFmpegFormat(variant<PixelFormat, SampleFormat> format) {
  switch (get<SampleFormat>(format)) {
    case SampleFormat::PackedU8:
      return AV_SAMPLE_FMT_U8;
    case SampleFormat::PackedS16:
      return AV_SAMPLE_FMT_S16;
    case SampleFormat::PackedS32:
      return AV_SAMPLE_FMT_S32;
    case SampleFormat::PackedS64:
      return AV_SAMPLE_FMT_S64;
    case SampleFormat::PackedFloat:
      return AV_SAMPLE_FMT_FLT;
    case SampleFormat::PackedDouble:
      return AV_SAMPLE_FMT_DBL;

    case SampleFormat::PlanarU8:
      return AV_SAMPLE_FMT_U8P;
    case SampleFormat::PlanarS16:
      return AV_SAMPLE_FMT_S16P;
    case SampleFormat::PlanarS32:
      return AV_SAMPLE_FMT_S32P;
    case SampleFormat::PlanarS64:
      return AV_SAMPLE_FMT_S64P;
    case SampleFormat::PlanarFloat:
      return AV_SAMPLE_FMT_FLTP;
    case SampleFormat::PlanarDouble:
      return AV_SAMPLE_FMT_DBLP;

    default:
      LOG(DFATAL) << "Unknown audio sample format: "
                  << static_cast<int>(get<SampleFormat>(format));
      return AV_SAMPLE_FMT_NONE;
  }
}

int64_t GetChannelLayout(int num_channels) {
  // See |channels| in https://wiki.libsdl.org/SDL_AudioSpec.
  switch (num_channels) {
    case 1:
      return AV_CH_LAYOUT_MONO;
    case 2:
      return AV_CH_LAYOUT_STEREO;
    case 4:
      return AV_CH_LAYOUT_QUAD;
    case 6:
      return AV_CH_LAYOUT_5POINT1;

    default:
      LOG(DFATAL) << "Unsupported channel count: " << num_channels;
      return AV_CH_LAYOUT_STEREO;
  }
}

}  // namespace

class SdlAudioRenderer::Impl {
 public:
  explicit Impl(const std::string& device_name)
      : mutex_("SdlAudioRenderer"),
        on_reset_("SdlAudioRenderer"),
        device_name_(device_name),
        player_(nullptr),
        input_(nullptr),
        audio_device_(0),
        swr_ctx_(nullptr),
        cur_time_(0),
        volume_(1),
        muted_(false),
        shutdown_(false),
        need_reset_(true),
        thread_("SdlAudio", std::bind(&Impl::ThreadMain, this)) {}

  ~Impl() {
    {
      std::unique_lock<Mutex> lock(mutex_);
      shutdown_ = true;
    }
    on_reset_.SignalAllIfNotSet();
    thread_.join();

    if (audio_device_ > 0)
      SDL_CloseAudioDevice(audio_device_);
    swr_free(&swr_ctx_);
  }

  NON_COPYABLE_OR_MOVABLE_TYPE(Impl);

  void OnSeek() {
    std::unique_lock<Mutex> lock(mutex_);
    cur_time_ = -1;
  }

  void SetPlayer(const MediaPlayer* player) {
    std::unique_lock<Mutex> lock(mutex_);
    player_ = player;
    on_reset_.SignalAllIfNotSet();
  }

  void Attach(const DecodedStream* stream) {
    std::unique_lock<Mutex> lock(mutex_);
    input_ = stream;
    on_reset_.SignalAllIfNotSet();
  }

  void Detach() {
    std::unique_lock<Mutex> lock(mutex_);
    input_ = nullptr;
  }

  double Volume() const {
    std::unique_lock<Mutex> lock(mutex_);
    return volume_;
  }

  void SetVolume(double volume) {
    std::unique_lock<Mutex> lock(mutex_);
    volume_ = volume;
    if (swr_ctx_) {
      av_opt_set_double(swr_ctx_, "rematrix_volume", muted_ ? 0 : volume_, 0);
      swr_init(swr_ctx_);
    }
  }

  bool Muted() const {
    std::unique_lock<Mutex> lock(mutex_);
    return muted_;
  }

  void SetMuted(bool muted) {
    std::unique_lock<Mutex> lock(mutex_);
    muted_ = muted;
    if (swr_ctx_) {
      av_opt_set_double(swr_ctx_, "rematrix_volume", muted_ ? 0 : volume_, 0);
      swr_init(swr_ctx_);
    }
  }

 private:
  void ThreadMain() {
    std::unique_lock<Mutex> lock(mutex_);
    while (!shutdown_) {
      if (need_reset_ && player_ && input_) {
        if (audio_device_ != 0) {
          SDL_CloseAudioDevice(audio_device_);
          audio_device_ = 0;
        }

        cur_time_ = player_->CurrentTime();
        auto frame = input_->GetFrame(cur_time_, FrameLocation::After);
        if (!frame) {
          util::Unlocker<Mutex> unlock(&lock);
          util::Clock::Instance.SleepSeconds(0.01);
          continue;
        }

        if (!InitDevice(frame.get()))
          return;

        SDL_PauseAudioDevice(audio_device_, 0);
        need_reset_ = false;
      }

      on_reset_.ResetAndWaitWhileUnlocked(lock);
    }
  }

  bool InitDevice(const DecodedFrame* frame) {
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
      SDL_SetMainReady();
      if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        LOG(ERROR) << "Error initializing SDL: " << SDL_GetError();
        return false;
      }
    }

    memset(&audio_spec_, 0, sizeof(audio_spec_));
    audio_spec_.freq = frame->sample_rate;
    audio_spec_.format = SDLFormatFromShaka(frame->format);
    audio_spec_.channels = static_cast<Uint8>(frame->channel_count);
    audio_spec_.samples = static_cast<Uint16>(frame->sample_count);
    audio_spec_.callback = &OnAudioCallback;
    audio_spec_.userdata = this;
    audio_device_ =
        SDL_OpenAudioDevice(nullptr, 0, &audio_spec_, &obtained_audio_spec_,
                            SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (audio_device_ == 0) {
      LOG(ERROR) << "Error opening audio device: " << SDL_GetError();
      return false;
    }

    // SDL may change the format so we get hardware acceleration.  Make sure
    // to use the format SDL expects.
    const AVSampleFormat av_sample_format =
        FFmpegFormatFromSDL(obtained_audio_spec_.format);
    if (av_sample_format == AV_SAMPLE_FMT_NONE)
      return false;

    const auto format = GetFFmpegFormat(frame->format);
    swr_ctx_ = swr_alloc_set_opts(
        swr_ctx_,
        GetChannelLayout(obtained_audio_spec_.channels),  // out_ch_layout
        av_sample_format,                                 // out_sample_fmt
        obtained_audio_spec_.freq,                        // out_sample_rate
        GetChannelLayout(frame->channel_count),           // in_ch_layout
        static_cast<AVSampleFormat>(format),              // in_sample_fmt
        frame->sample_rate,                               // in_sample_rate
        0,                                                // log_offset,
        nullptr);                                         // log_ctx
    if (!swr_ctx_) {
      LOG(ERROR) << "Unable to allocate swrescale context.";
      return false;
    }

    // Minimum difference before changing samples to match timestamps.
    av_opt_set_double(swr_ctx_, "min_comp", 0.01, 0);
    // Maximum factor to adjust existing samples by.
    av_opt_set_double(swr_ctx_, "max_soft_comp", 0.01, 0);
    // Minimum difference before applying hard compensation (adding/dropping
    // samples).
    av_opt_set_double(swr_ctx_, "min_hard_comp", 0.1, 0);
    // Sync samples to timestamps.
    av_opt_set_double(swr_ctx_, "async", 1, 0);
    // Change volume to this value.
    av_opt_set_double(swr_ctx_, "rematrix_volume", muted_ ? 0 : volume_, 0);

    swr_init(swr_ctx_);
    return true;
  }

  static void OnAudioCallback(void* user_data, uint8_t* data, int size) {
    reinterpret_cast<Impl*>(user_data)->AudioCallback(data, size);
  }

  void AudioCallback(uint8_t* data, int size) {
    std::unique_lock<Mutex> lock(mutex_);

    if (!player_ || !input_) {
      need_reset_ = true;
      // Don't trigger on_reset_ since it will just wait anyway.  Once we get a
      // player or input it will trigger on_reset_ and the reset will happen.
      // Until then, we just play silence.
      memset(data, obtained_audio_spec_.silence, size);
      return;
    }

    const double playback_rate = player_->PlaybackRate();
    // TODO: Support playback rate by using atemp filter.
    DCHECK(playback_rate == 0 || playback_rate == 1)
        << "Only playbackRate of 0 and 1 are supported.";
    if (need_reset_ || volume_ == 0 || playback_rate <= 0 ||
        playback_rate > kMaxPlaybackRate ||
        player_->PlaybackState() != VideoPlaybackState::Playing) {
      memset(data, obtained_audio_spec_.silence, size);
      return;
    }

    const AVSampleFormat av_sample_format =
        FFmpegFormatFromSDL(obtained_audio_spec_.format);
    const int sample_size = av_get_bytes_per_sample(av_sample_format) *
                            obtained_audio_spec_.channels;
    int size_in_samples = size / sample_size;
    DCHECK_EQ(size % sample_size, 0);

    const double now_time = player_->CurrentTime();
    if (cur_time_ >= 0) {
      // |cur_time_ - delay| represents the playhead time that is about to be
      // played.
      const double delay = swr_get_delay(swr_ctx_, 1000) / 1000.0;
      if (cur_time_ - delay < now_time - kMaxAudioDelay) {
        // The next frame being played is from too long ago; so simulate a seek
        // to play the audio at the playhead.
        cur_time_ = -1;
      }
    }

    if (cur_time_ < 0) {
      cur_time_ = now_time;
      // swr will adjust samples to match their expected timestamps; reset the
      // context on seek so it doesn't break with the new timestamps.
      swr_init(swr_ctx_);
    }

    // Flush existing data before reading more frames.
    const int initial_sample_count =
        swr_convert(swr_ctx_, &data, size_in_samples, nullptr, 0);
    if (initial_sample_count < 0) {
      memset(data, 0, size);
      return;
    }
    DCHECK_LE(initial_sample_count, size_in_samples);
    size_in_samples -= initial_sample_count;
    data += initial_sample_count * sample_size;

    while (size_in_samples > 0) {
      auto frame = input_->GetFrame(cur_time_, FrameLocation::After);
      if (!frame)
        break;

      // If the source changed, we need to reset.  If the new frame has a lower
      // sample rate or channel count, we can just use swresample to change
      // these.  If they are higher, we want to try to create a new device so we
      // get the benefits.
      if (frame->sample_rate > static_cast<uint32_t>(audio_spec_.freq) ||
          frame->channel_count > audio_spec_.channels ||
          SDLFormatFromShaka(frame->format) != audio_spec_.format) {
        need_reset_ = true;
        on_reset_.SignalAll();
        break;
      }
      if (frame->sample_rate != static_cast<uint32_t>(audio_spec_.freq)) {
        av_opt_set_int(swr_ctx_, "in_sample_rate", frame->sample_rate, 0);
        swr_init(swr_ctx_);
        audio_spec_.freq = frame->sample_rate;
      }
      if (frame->channel_count != audio_spec_.channels) {
        av_opt_set_int(swr_ctx_, "in_channel_layout",
                       GetChannelLayout(frame->channel_count), 0);
        swr_init(swr_ctx_);
        audio_spec_.channels = static_cast<Uint8>(frame->channel_count);
      }

      // Assume the first byte in the array will be played "right-now", or at
      // |now_time|.  This is technically not correct, but the delay shouldn't
      // be noticeable.
      const auto pts = static_cast<uint64_t>(
          frame->pts * obtained_audio_spec_.freq * frame->sample_rate);
      // Swr will adjust the audio so the next sample will happen at |pts|.
      if (swr_next_pts(swr_ctx_, pts) < 0)
        break;

      const int samples_read = swr_convert(
          swr_ctx_, &data, size_in_samples,
          const_cast<const uint8_t**>(frame->data.data()),  // NOLINT
          frame->sample_count);
      if (samples_read < 0)
        break;

      DCHECK_LE(samples_read, size_in_samples);
      size_in_samples -= samples_read;
      data += samples_read * sample_size;

      cur_time_ = frame->pts;
    }

    // Set any remaining data to silence in the event of errors.
    memset(data, obtained_audio_spec_.silence, size_in_samples * sample_size);
  }

  mutable Mutex mutex_;
  ThreadEvent<void> on_reset_;

  const std::string device_name_;
  const MediaPlayer* player_;
  const DecodedStream* input_;

  SDL_AudioSpec audio_spec_;
  SDL_AudioSpec obtained_audio_spec_;
  SDL_AudioDeviceID audio_device_;
  SwrContext* swr_ctx_;
  double cur_time_;
  double volume_;
  bool muted_;
  bool shutdown_ : 1;
  bool need_reset_ : 1;

  Thread thread_;
};


SdlAudioRenderer::SdlAudioRenderer(const std::string& device_name)
    : impl_(new Impl(device_name)) {}
SdlAudioRenderer::~SdlAudioRenderer() {}

void SdlAudioRenderer::OnSeek() {
  impl_->OnSeek();
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
