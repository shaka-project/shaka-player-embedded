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

#include "src/media/audio_renderer.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <algorithm>
#include <utility>

#include "src/media/ffmpeg_decoded_frame.h"
#include "src/util/clock.h"
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
constexpr const double kMaxDelay = 0.2;


SDL_AudioFormat SDLFormatFromFFmpeg(AVSampleFormat format) {
  // Try to use the same format to avoid work by swresample.
  switch (format) {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_U8P:
      return AUDIO_U8;
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S16P:
      return AUDIO_S16SYS;
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_S32P:
      return AUDIO_S32SYS;
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_FLTP:
      return AUDIO_F32SYS;

    case AV_SAMPLE_FMT_DBL:
    case AV_SAMPLE_FMT_DBLP: {
      LOG_ONCE(WARNING) << "SDL doesn't support double-precision audio "
                        << "formats, converting to floats.";
      return AUDIO_F32SYS;
    }
    case AV_SAMPLE_FMT_S64:
    case AV_SAMPLE_FMT_S64P: {
      LOG_ONCE(WARNING) << "SDL doesn't support 64-bit audio "
                        << "formats, converting to 32-bit.";
      return AUDIO_S32SYS;
    }

    default:
      LOG(DFATAL) << "Unknown audio sample format: " << format;
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

AudioRenderer::AudioRenderer(std::function<double()> get_time,
                             std::function<double()> get_playback_rate,
                             Stream* stream)
    : get_time_(std::move(get_time)),
      get_playback_rate_(std::move(get_playback_rate)),
      stream_(stream),
      mutex_("AudioRenderer"),
      on_reset_("Reset AudioRenderer"),
      audio_device_(0),
      swr_ctx_(nullptr),
      cur_time_(-1),
      volume_(1),
      shutdown_(false),
      need_reset_(true),
      is_seeking_(false),
      thread_("AudioRenderer", std::bind(&AudioRenderer::ThreadMain, this)) {}

AudioRenderer::~AudioRenderer() {
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

void AudioRenderer::OnSeek() {
  std::unique_lock<Mutex> lock(mutex_);
  is_seeking_ = true;
  cur_time_ = -1;
}

void AudioRenderer::OnSeekDone() {
  std::unique_lock<Mutex> lock(mutex_);
  is_seeking_ = false;

  // Now that the seek is done, discard frames from the old time.
  const double time = get_time_();
  stream_->GetDecodedFrames()->Remove(0, time - 3);
  stream_->GetDecodedFrames()->Remove(time + 3, HUGE_VAL);
}

void AudioRenderer::SetVolume(double volume) {
  std::unique_lock<Mutex> lock(mutex_);
  volume_ = volume;
  if (swr_ctx_) {
    av_opt_set_double(swr_ctx_, "rematrix_volume", volume_, 0);
    swr_init(swr_ctx_);
  }
}

void AudioRenderer::ThreadMain() {
  std::unique_lock<Mutex> lock(mutex_);
  while (!shutdown_) {
    if (need_reset_) {
      if (audio_device_ != 0) {
        SDL_CloseAudioDevice(audio_device_);
        audio_device_ = 0;
      }

      cur_time_ = get_time_();
      auto base_frame = stream_->GetDecodedFrames()->GetFrameAfter(cur_time_);
      if (!base_frame) {
        util::Unlocker<Mutex> unlock(&lock);
        util::Clock::Instance.SleepSeconds(0.01);
        continue;
      }

      auto* frame = static_cast<const FFmpegDecodedFrame*>(base_frame.get());

      if (!InitDevice(frame))
        return;

      SDL_PauseAudioDevice(audio_device_, 0);
      need_reset_ = false;
    }

    on_reset_.ResetAndWaitWhileUnlocked(lock);
  }
}

bool AudioRenderer::InitDevice(const FFmpegDecodedFrame* frame) {
  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    SDL_SetMainReady();
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
      LOG(ERROR) << "Error initializing SDL: " << SDL_GetError();
      return false;
    }
  }

  memset(&audio_spec_, 0, sizeof(audio_spec_));
  audio_spec_.freq = frame->raw_frame()->sample_rate;
  audio_spec_.format = SDLFormatFromFFmpeg(frame->sample_format());
  audio_spec_.channels = static_cast<Uint8>(frame->raw_frame()->channels);
  audio_spec_.samples = static_cast<Uint16>(frame->raw_frame()->nb_samples *
                                            frame->raw_frame()->channels);
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

  swr_ctx_ = swr_alloc_set_opts(
      swr_ctx_,
      GetChannelLayout(obtained_audio_spec_.channels),  // out_ch_layout
      av_sample_format,                                 // out_sample_fmt
      obtained_audio_spec_.freq,                        // out_sample_rate
      frame->raw_frame()->channel_layout,               // in_ch_layout
      frame->sample_format(),                           // in_sample_fmt
      frame->raw_frame()->sample_rate,                  // in_sample_rate
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
  av_opt_set_double(swr_ctx_, "rematrix_volume", volume_, 0);

  swr_init(swr_ctx_);
  return true;
}

// static
void AudioRenderer::OnAudioCallback(void* user_data, uint8_t* data, int size) {
  reinterpret_cast<AudioRenderer*>(user_data)->AudioCallback(data, size);
}

void AudioRenderer::AudioCallback(uint8_t* data, int size) {
  std::unique_lock<Mutex> lock(mutex_);

  if (cur_time_ >= 0)
    stream_->GetDecodedFrames()->Remove(0, cur_time_ - 0.2);

  const double playback_rate = get_playback_rate_();
  // TODO: Support playback rate by using atemp filter.
  DCHECK(playback_rate == 0 || playback_rate == 1)
      << "Only playbackRate of 0 and 1 are supported.";
  if (need_reset_ || is_seeking_ || volume_ == 0 || playback_rate <= 0 ||
      playback_rate > kMaxPlaybackRate) {
    memset(data, obtained_audio_spec_.silence, size);
    return;
  }

  const AVSampleFormat av_sample_format =
      FFmpegFormatFromSDL(obtained_audio_spec_.format);
  const int sample_size =
      av_get_bytes_per_sample(av_sample_format) * obtained_audio_spec_.channels;
  int size_in_samples = size / sample_size;
  DCHECK_EQ(size % sample_size, 0);

  const double now_time = get_time_();
  if (cur_time_ >= 0) {
    // |cur_time_ - delay| represents the playhead time that is about to be
    // played.
    const double delay = swr_get_delay(swr_ctx_, 1000) / 1000.0;
    if (cur_time_ - delay < now_time - kMaxDelay) {
      // The next frame being played is from too long ago; so simulate a seek to
      // play the audio at the playhead.
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
    auto base_frame = stream_->GetDecodedFrames()->GetFrameAfter(cur_time_);
    if (!base_frame)
      break;

    auto* frame = static_cast<const FFmpegDecodedFrame*>(base_frame.get());

    // If the source changed, we need to reset.  If the new frame has a lower
    // sample rate or channel count, we can just use swresample to change
    // these.  If they are higher, we want to try to create a new device so we
    // get the benefits.
    if (frame->raw_frame()->sample_rate > audio_spec_.freq ||
        frame->raw_frame()->channels > audio_spec_.channels ||
        SDLFormatFromFFmpeg(frame->sample_format()) != audio_spec_.format) {
      need_reset_ = true;
      on_reset_.SignalAll();
      break;
    }
    if (frame->raw_frame()->sample_rate != audio_spec_.freq) {
      av_opt_set_int(swr_ctx_, "in_sample_rate",
                     frame->raw_frame()->sample_rate, 0);
      swr_init(swr_ctx_);
      audio_spec_.freq = frame->raw_frame()->sample_rate;
    }
    if (frame->raw_frame()->channels != audio_spec_.channels) {
      av_opt_set_int(swr_ctx_, "in_channel_layout",
                     GetChannelLayout(frame->raw_frame()->channels), 0);
      swr_init(swr_ctx_);
      audio_spec_.channels = static_cast<Uint8>(frame->raw_frame()->channels);
    }

    // Assume the first byte in the array will be played "right-now", or at
    // |now_time|.  This is technically not correct, but the delay shouldn't be
    // noticeable.
    const auto pts =
        static_cast<uint64_t>(frame->pts * obtained_audio_spec_.freq *
                              frame->raw_frame()->sample_rate);
    // Swr will adjust the audio so the next sample will happen at |pts|.
    if (swr_next_pts(swr_ctx_, pts) < 0)
      break;

    const int samples_read =
        swr_convert(swr_ctx_, &data, size_in_samples,
                    const_cast<const uint8_t**>(frame->data()),  // NOLINT
                    frame->raw_frame()->nb_samples);
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

}  // namespace media
}  // namespace shaka
