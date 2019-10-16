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

#include <chrono>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "src/media/media_processor.h"
#include "src/media/pipeline_manager.h"
#include "src/media/stream.h"

namespace shaka {
namespace media {

/** The number of seconds to keep decoded ahead of the playhead. */
constexpr const double kDecodeBufferSize = 1;

/** The number of seconds gap before we assume we are at the end. */
constexpr const double kEndDelta = 0.1;

DecoderThread::DecoderThread(std::function<double()> get_time,
                             std::function<void()> seek_done,
                             std::function<void()> on_waiting_for_key,
                             std::function<void(Status)> on_error,
                             MediaProcessor* processor,
                             PipelineManager* pipeline, Stream* stream)
    : processor_(processor),
      pipeline_(pipeline),
      stream_(stream),
      get_time_(std::move(get_time)),
      seek_done_(std::move(seek_done)),
      on_waiting_for_key_(std::move(on_waiting_for_key)),
      on_error_(std::move(on_error)),
      cdm_(nullptr),
      shutdown_(false),
      is_seeking_(false),
      did_flush_(false),
      last_frame_time_(NAN),
      raised_waiting_event_(false),
      thread_(processor->codec() + " decoder",
              std::bind(&DecoderThread::ThreadMain, this)) {}

DecoderThread::~DecoderThread() {
  CHECK(!thread_.joinable()) << "Need to call Stop() before destroying";
}

void DecoderThread::Stop() {
  shutdown_.store(true, std::memory_order_release);
  thread_.join();
}

void DecoderThread::OnSeek() {
  last_frame_time_.store(NAN, std::memory_order_release);
  is_seeking_.store(true, std::memory_order_release);
  did_flush_.store(false, std::memory_order_release);
}

void DecoderThread::SetCdm(eme::Implementation* cdm) {
  cdm_.store(cdm, std::memory_order_release);
}

void DecoderThread::ThreadMain() {
  while (!shutdown_.load(std::memory_order_acquire)) {
    const double cur_time = get_time_();
    double last_time = last_frame_time_.load(std::memory_order_acquire);

    std::shared_ptr<EncodedFrame> frame;
    if (std::isnan(last_time)) {
      processor_->ResetDecoder();
      frame = stream_->GetDemuxedFrames()->GetFrame(
          cur_time, FrameLocation::KeyFrameBefore);
    } else {
      frame = stream_->GetDemuxedFrames()->GetFrame(last_time,
                                                    FrameLocation::After);
    }

    if (stream_->DecodedAheadOf(cur_time) > kDecodeBufferSize) {
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
      continue;
    }
    if (!frame) {
      if (!std::isnan(last_time) &&
          last_time + kEndDelta >= pipeline_->GetDuration() &&
          !did_flush_.load(std::memory_order_acquire)) {
        // If this is the last frame, pass the null to DecodeFrame, which will
        // flush the decoder.
        did_flush_.store(true, std::memory_order_release);
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        continue;
      }
    }

    std::vector<std::shared_ptr<DecodedFrame>> decoded;
    eme::Implementation* cdm = cdm_.load(std::memory_order_acquire);
    const Status decode_status =
        processor_->DecodeFrame(cur_time, frame, cdm, &decoded);
    if (decode_status == Status::KeyNotFound) {
      // If we don't have the required key, signal the <video> and wait.
      if (!raised_waiting_event_) {
        raised_waiting_event_ = true;
        on_waiting_for_key_();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }
    if (decode_status != Status::Success) {
      on_error_(decode_status);
      break;
    }

    raised_waiting_event_ = false;
    const double last_pts = decoded.empty() ? -1 : decoded.back()->pts;
    for (auto& decoded_frame : decoded) {
      stream_->GetDecodedFrames()->AddFrame(decoded_frame);
    }

    if (frame) {
      // Don't change the |last_frame_time_| if it was reset to NAN while this
      // was running.
      const bool updated = last_frame_time_.compare_exchange_strong(
          last_time, frame->dts, std::memory_order_acq_rel);
      if (updated && last_pts >= cur_time) {
        bool expected = true;
        if (is_seeking_.compare_exchange_strong(expected, false,
                                                std::memory_order_acq_rel)) {
          seek_done_();
        }
      }
    }
  }
}

}  // namespace media
}  // namespace shaka
