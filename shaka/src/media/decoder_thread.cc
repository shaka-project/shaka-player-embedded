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

#include "src/media/decoder_thread.h"

#include <glog/logging.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "src/util/clock.h"
#include "src/util/utils.h"

namespace shaka {
namespace media {

namespace {

/** The number of seconds to keep decoded ahead of the playhead. */
constexpr const double kDecodeBufferSize = 1;

/** The number of seconds gap before we assume we are at the end. */
constexpr const double kEndDelta = 0.1;

double DecodedAheadOf(StreamBase* stream, double time) {
  for (auto& range : stream->GetBufferedRanges()) {
    if (range.end > time) {
      if (range.start < time + StreamBase::kMaxGapSize) {
        return range.end - std::max(time, range.start);
      }

      // The ranges are sorted, so avoid looking at the remaining elements.
      break;
    }
  }
  return 0;
}

}  // namespace

DecoderThread::DecoderThread(Client* client, DecodedStream* output)
    : mutex_("DecoderThread"),
      signal_("DecoderChanged"),
      client_(client),
      input_(nullptr),
      output_(output),
      decoder_(nullptr),
      cdm_(nullptr),
      last_frame_time_(NAN),
      shutdown_(false),
      did_flush_(false),
      raised_waiting_event_(false),
      thread_("Decoder", std::bind(&DecoderThread::ThreadMain, this)) {}

DecoderThread::~DecoderThread() {
  {
    std::unique_lock<Mutex> lock(mutex_);
    shutdown_ = true;
    signal_.SignalAllIfNotSet();
  }
  thread_.join();
}

void DecoderThread::Attach(const ElementaryStream* input) {
  std::unique_lock<Mutex> lock(mutex_);
  input_ = input;
  if (input && decoder_)
    signal_.SignalAllIfNotSet();
}

void DecoderThread::Detach() {
  std::unique_lock<Mutex> lock(mutex_);
  input_ = nullptr;
}

void DecoderThread::OnSeek() {
  std::unique_lock<Mutex> lock(mutex_);
  last_frame_time_ = NAN;
  did_flush_ = false;
  // Remove all the existing frames.  We'll decode them again anyway and this
  // ensures we don't keep future frames forever when seeking backwards.
  output_->Remove(0, INFINITY);
}

void DecoderThread::SetCdm(eme::Implementation* cdm) {
  std::unique_lock<Mutex> lock(mutex_);
  cdm_ = cdm;
}

void DecoderThread::SetDecoder(Decoder* decoder) {
  std::unique_lock<Mutex> lock(mutex_);
  decoder_ = decoder;
  if (decoder && input_)
    signal_.SignalAllIfNotSet();
}

void DecoderThread::ThreadMain() {
  std::unique_lock<Mutex> lock(mutex_);
  while (!shutdown_) {
    if (!input_ || !decoder_) {
      if (input_)
        LOG(DFATAL) << "No decoder provided and no default decoder exists";

      signal_.ResetAndWaitWhileUnlocked(lock);
      continue;
    }

    const double cur_time = client_->CurrentTime();
    double last_time = last_frame_time_;

    if (DecodedAheadOf(output_, cur_time) > kDecodeBufferSize) {
      util::Unlocker<Mutex> unlock(&lock);
      util::Clock::Instance.SleepSeconds(0.025);
      continue;
    }

    // Evict frames that are not near the current time.  This ensures we don't
    // keep frames buffered forever.
    output_->Remove(0, cur_time - kDecodeBufferSize);

    std::shared_ptr<EncodedFrame> frame;
    if (std::isnan(last_time)) {
      decoder_->ResetDecoder();
      // Move the time forward a bit to allow gaps at the start.  This will move
      // backward to find a keyframe anyway.
      frame = input_->GetFrame(cur_time + StreamBase::kMaxGapSize,
                               FrameLocation::KeyFrameBefore);
    } else {
      frame = input_->GetFrame(last_time, FrameLocation::After);
    }

    if (!frame) {
      if (!std::isnan(last_time) &&
          last_time + kEndDelta >= client_->Duration() && !did_flush_) {
        // If this is the last frame, pass the null to DecodeFrame, which will
        // flush the decoder.
        did_flush_ = true;
      } else {
        util::Unlocker<Mutex> unlock(&lock);
        util::Clock::Instance.SleepSeconds(0.025);
        continue;
      }
    }

    std::string error;
    std::vector<std::shared_ptr<DecodedFrame>> decoded;
    const MediaStatus decode_status =
        decoder_->Decode(frame, cdm_, &decoded, &error);
    if (decode_status == MediaStatus::KeyNotFound) {
      // If we don't have the required key, signal the <video> and wait.
      if (!raised_waiting_event_) {
        raised_waiting_event_ = true;
        client_->OnWaitingForKey();
      }
      // TODO: Consider adding a signal for new keys so we can avoid polling and
      // just wait on a condition variable.
      util::Unlocker<Mutex> unlock(&lock);
      util::Clock::Instance.SleepSeconds(0.2);
      continue;
    }
    if (decode_status != MediaStatus::Success) {
      client_->OnError(error);
      break;
    }

    raised_waiting_event_ = false;
    for (auto& decoded_frame : decoded) {
      output_->AddFrame(decoded_frame);
    }

    if (frame)
      last_frame_time_ = frame->dts;
  }
}

}  // namespace media
}  // namespace shaka
