// Copyright 2020 Google LLC
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

#include "shaka/media/apple_audio_renderer.h"

#include <AudioToolbox/AudioToolbox.h>
#include <glog/logging.h>

#include <list>

#include "src/debug/mutex.h"
#include "src/media/audio_renderer_common.h"
#include "src/media/media_utils.h"
#include "src/util/cfref.h"

namespace shaka {

namespace util {

// Add a specialization so a CFRef<T> can be used with a AudioQueue.
template <>
struct RefTypeTraits<AudioQueueRef> {
  static constexpr const bool AcquireWithRaw = false;

  static AudioQueueRef Duplicate(AudioQueueRef arg) {
    // Cannot duplicate queue objects.
    LOG(FATAL) << "Not reached";
  }

  static void Release(AudioQueueRef arg) {
    AudioQueueDispose(arg, /* inImmediate= */ true);
  }
};

}  // namespace util

namespace media {

namespace {

/** The maximum number of buffers to keep. */
constexpr const size_t kMaxNumBuffers = 8;

bool SetSampleFormatFields(SampleFormat format,
                           AudioStreamBasicDescription* desc) {
  switch (format) {
    case SampleFormat::PackedU8:
      desc->mFormatFlags = 0;
      desc->mBitsPerChannel = 8;
      return true;
    case SampleFormat::PackedS16:
      desc->mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
      desc->mBitsPerChannel = 16;
      return true;
    case SampleFormat::PackedS32:
      desc->mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
      desc->mBitsPerChannel = 32;
      return true;
    case SampleFormat::PackedS64:
      desc->mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
      desc->mBitsPerChannel = 64;
      return true;
    case SampleFormat::PackedFloat:
      desc->mFormatFlags = kLinearPCMFormatFlagIsFloat;
      desc->mBitsPerChannel = 32;
      return true;
    case SampleFormat::PackedDouble:
      desc->mFormatFlags = kLinearPCMFormatFlagIsFloat;
      desc->mBitsPerChannel = 64;
      return true;

    case SampleFormat::PlanarU8:
    case SampleFormat::PlanarS16:
    case SampleFormat::PlanarS32:
    case SampleFormat::PlanarS64:
    case SampleFormat::PlanarFloat:
    case SampleFormat::PlanarDouble:
      LOG(DFATAL) << "Audio renderer doesn't support planar formats";
      return false;
    default:
      LOG(DFATAL) << "Unknown sample format: " << static_cast<uint8_t>(format);
      return false;
  }
}

/**
 * Contains a list of buffers for use with an AudioQueue.  Buffers are created
 * for a specific queue and are stored by this class until they need to be used.
 * Once we give up a buffer, the audio device will queue that buffer to be
 * played.  Once the device has played the data, the buffer will be returned to
 * this class to be reused.
 */
class BufferList final {
 public:
  BufferList() : queue_(nullptr) {}
  ~BufferList() {
    Reset();
  }

  void SetQueue(AudioQueueRef q) {
    Reset();
    queue_ = q;
  }

  AudioQueueBufferRef FindBuffer(size_t size) {
    // Look for a buffer that is the exact requested size; if there isn't one
    // available, look for one that is larger than needed.
    auto found = buffers_.end();
    auto capacity = [](decltype(found) it) {
      return (*it)->mAudioDataBytesCapacity;
    };
    for (auto it = buffers_.begin(); it != buffers_.end(); it++) {
      if (capacity(it) == size) {
        found = it;
        break;
      } else if (capacity(it) > size &&
                 (found == buffers_.end() || capacity(it) < capacity(found))) {
        // Use the smallest buffer we can, but favor earlier elements.
        found = it;
      }
    }

    if (found != buffers_.end()) {
      auto* ret = *found;
      buffers_.erase(found);
      return ret;
    }

    AudioQueueBufferRef ret;
    const auto status = AudioQueueAllocateBuffer(queue_, size, &ret);
    if (status != 0) {
      LOG(DFATAL) << "Error creating AudioQueueBuffer: " << status;
      return nullptr;
    }
    return ret;
  }

  void ReuseBuffer(AudioQueueBufferRef buffer) {
    buffers_.emplace_front(buffer);
    while (buffers_.size() > kMaxNumBuffers) {
      FreeBuffer(buffers_.back());
      buffers_.pop_back();
    }
  }

 private:
  void Reset() {
    for (auto* buf : buffers_)
      FreeBuffer(buf);
    buffers_.clear();
  }

  void FreeBuffer(AudioQueueBufferRef buffer) {
    const auto status = AudioQueueFreeBuffer(queue_, buffer);
    if (status != 0)
      LOG(DFATAL) << "Error freeing AudioQueueBuffer: " << status;
  }

  AudioQueueRef queue_;
  std::list<AudioQueueBufferRef> buffers_;
};

}  // namespace

class AppleAudioRenderer::Impl : public AudioRendererCommon {
 public:
  Impl() : mutex_("AudioRenderer") {}

  ~Impl() override {
    Stop();
    // Clear the buffer so we get all our buffers back from the device so the
    // BufferList can free them.  This ensures we free the buffers before our
    // members are destructed.
    ClearBuffer();
  }

  bool InitDevice(std::shared_ptr<DecodedFrame> frame, double volume) override {
    std::unique_lock<Mutex> lock(mutex_);
    AudioStreamBasicDescription desc = {0};
    if (!SetSampleFormatFields(get<SampleFormat>(frame->format), &desc))
      return false;
    desc.mFormatID = kAudioFormatLinearPCM;
    desc.mSampleRate = frame->stream_info->sample_rate;
    desc.mChannelsPerFrame = frame->stream_info->channel_count;
    desc.mFramesPerPacket = 1;
    desc.mBytesPerFrame = desc.mBitsPerChannel * desc.mChannelsPerFrame / 8;
    desc.mBytesPerPacket = desc.mBytesPerFrame * desc.mFramesPerPacket;

    AudioQueueRef q;
    auto status = AudioQueueNewOutput(
        &desc, &OnAudioCallback, this, /* inCallbackRunLoop= */ nullptr,
        /* inCallbackRunLoopMode= */ nullptr, /* inFlags= */ 0, &q);
    if (status != 0) {
      LOG(DFATAL) << "Error creating AudioQueue: " << status;
      return false;
    }

    queue_ = q;
    queue_size_ = 0;
    buffers_.SetQueue(q);
    UpdateVolume(volume);
    return true;
  }

  bool AppendBuffer(const uint8_t* data, size_t size) override {
    std::unique_lock<Mutex> lock(mutex_);
    auto* buffer = buffers_.FindBuffer(size);
    if (!buffer)
      return false;

    memcpy(buffer->mAudioData, data, size);
    queue_size_ += size;
    buffer->mAudioDataByteSize = size;
    buffer->mPacketDescriptionCount = 0;
    const auto status = AudioQueueEnqueueBuffer(queue_, buffer, 0, nullptr);
    if (status != 0)
      LOG(DFATAL) << "Error queuing AudioQueueBuffer: " << status;
    return status == 0;
  }

  void ClearBuffer() override {
    std::unique_lock<Mutex> lock(mutex_);
    if (!queue_)
      return;

    queue_size_ = 0;
    const auto status = AudioQueueReset(queue_);
    if (status != 0)
      LOG(DFATAL) << "Error clearing AudioQueue: " << status;
  }

  size_t GetBytesBuffered() const override {
    std::unique_lock<Mutex> lock(mutex_);
    return queue_size_;
  }

  void SetDeviceState(bool is_playing) override {
    std::unique_lock<Mutex> lock(mutex_);
    if (!queue_)
      return;

    OSStatus status;
    if (is_playing)
      status = AudioQueueStart(queue_, nullptr);
    else
      status = AudioQueuePause(queue_);
    if (status != 0)
      LOG(DFATAL) << "Error changing AudioQueue pause state: " << status;
  }

  void UpdateVolume(double volume) override {
    if (!queue_)
      return;

    const auto status = AudioQueueSetParameter(queue_, kAudioQueueParam_Volume,
                                               static_cast<float>(volume));
    if (status != 0)
      LOG(DFATAL) << "Error setting AudioQueue volume: " << status;
  }

  static void OnAudioCallback(void* user_data, AudioQueueRef queue,
                              AudioQueueBufferRef buffer) {
    auto* impl = reinterpret_cast<Impl*>(user_data);
    std::unique_lock<Mutex> lock(impl->mutex_);
    DCHECK_EQ(queue, impl->queue_);

    // Now that the buffer is returned to us, it is no longer used by the audio
    // device, so the buffer size should be reduced.
    if (buffer->mAudioDataByteSize > impl->queue_size_)
      impl->queue_size_ = 0;
    else
      impl->queue_size_ -= buffer->mAudioDataByteSize;

    impl->buffers_.ReuseBuffer(buffer);
  }

  mutable Mutex mutex_;
  util::CFRef<AudioQueueRef> queue_;
  BufferList buffers_;
  // Tracks the number of bytes buffered by the audio device.
  size_t queue_size_;
};

AppleAudioRenderer::AppleAudioRenderer() : impl_(new Impl) {}

AppleAudioRenderer::~AppleAudioRenderer() {}

void AppleAudioRenderer::SetPlayer(const MediaPlayer* player) {
  impl_->SetPlayer(player);
}

void AppleAudioRenderer::Attach(const DecodedStream* stream) {
  impl_->Attach(stream);
}

void AppleAudioRenderer::Detach() {
  impl_->Detach();
}

double AppleAudioRenderer::Volume() const {
  return impl_->Volume();
}

void AppleAudioRenderer::SetVolume(double volume) {
  impl_->SetVolume(volume);
}

bool AppleAudioRenderer::Muted() const {
  return impl_->Muted();
}

void AppleAudioRenderer::SetMuted(bool muted) {
  impl_->SetMuted(muted);
}

}  // namespace media
}  // namespace shaka
