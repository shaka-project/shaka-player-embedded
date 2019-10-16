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

#include "src/media/demuxer_thread.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "src/core/js_manager_impl.h"
#include "src/media/media_processor.h"
#include "src/media/stream.h"

namespace shaka {
namespace media {

namespace {

std::string ShortContainerName(const std::string& container) {
  if (container == "matroska") {
    return "mkv";
  }

  DCHECK_LT(container.size(), 10u) << "Container needs a short name";
  return container.substr(0, 10);
}

}  // namespace

DemuxerThread::DemuxerThread(std::function<void()> on_load_meta,
                             MediaProcessor* processor, Stream* stream)
    : mutex_("DemuxerThread"),
      new_data_("New demuxed data"),
      on_load_meta_(std::move(on_load_meta)),
      shutdown_(false),
      cur_data_(nullptr),
      cur_size_(0),
      window_start_(0),
      window_end_(HUGE_VAL),
      need_key_frame_(true),
      processor_(processor),
      stream_(stream),
      thread_(ShortContainerName(processor->container()) + " demux",
              std::bind(&DemuxerThread::ThreadMain, this)) {}

DemuxerThread::~DemuxerThread() {
  CHECK(!thread_.joinable()) << "Need to call Stop() before destroying";
}

void DemuxerThread::Stop() {
  shutdown_ = true;
  new_data_.SignalAllIfNotSet();
  thread_.join();
}

void DemuxerThread::AppendData(double timestamp_offset, double window_start,
                               double window_end, const uint8_t* data,
                               size_t data_size,
                               std::function<void(Status)> on_complete) {
  DCHECK(data);
  DCHECK_GT(data_size, 0u);

  std::unique_lock<Mutex> lock(mutex_);
  DCHECK(input_.empty());  // Should not be performing an update.
  input_.SetBuffer(data, data_size);
  processor_->SetTimestampOffset(timestamp_offset);
  window_start_ = window_start;
  window_end_ = window_end;
  cur_data_ = data;
  cur_size_ = data_size;
  on_complete_ = std::move(on_complete);

  new_data_.SignalAll();
}

void DemuxerThread::ThreadMain() {
  using namespace std::placeholders;  // NOLINT
  auto on_read = std::bind(&DemuxerThread::OnRead, this, _1, _2);
  auto on_reset_read = std::bind(&DemuxerThread::OnResetRead, this);
  const Status init_status =
      processor_->InitializeDemuxer(on_read, on_reset_read);
  if (init_status != Status::Success) {
    // If we get an error before we append the first segment, then we won't have
    // a callback yet, so we have nowhere to send the error to.  Wait until we
    // get the first segment.
    std::unique_lock<Mutex> lock(mutex_);
    if (!on_complete_ && !shutdown_) {
      new_data_.ResetAndWaitWhileUnlocked(lock);
    }
    CallOnComplete(init_status);
    return;
  }

  // The demuxer initialization will read the init segment and load any metadata
  // it needs.
  on_load_meta_();

  while (!shutdown_) {
    std::shared_ptr<EncodedFrame> frame;
    const Status status = processor_->ReadDemuxedFrame(&frame);
    if (status != Status::Success) {
      std::unique_lock<Mutex> lock(mutex_);
      CallOnComplete(status);
      break;
    }

    {
      std::unique_lock<Mutex> lock(mutex_);
      if (frame->pts < window_start_ ||
          frame->pts + frame->duration > window_end_) {
        need_key_frame_ = true;
        VLOG(2) << "Dropping frame outside append window, pts=" << frame->pts;
        continue;
      }
      if (need_key_frame_) {
        if (frame->is_key_frame) {
          need_key_frame_ = false;
        } else {
          VLOG(2) << "Dropping frame while looking for key frame, pts="
                  << frame->pts;
          continue;
        }
      }
    }
    stream_->GetDemuxedFrames()->AddFrame(frame);
  }
}

size_t DemuxerThread::OnRead(uint8_t* data, size_t data_size) {
  std::unique_lock<Mutex> lock(mutex_);

  // If we get called with no input, then we are done processing the previous
  // data, so call on_complete.  This is a no-op if there is no callback.
  if (input_.empty()) {
    CallOnComplete(Status::Success);
    if (!shutdown_) {
      new_data_.ResetAndWaitWhileUnlocked(lock);
    }
  }

  if (shutdown_)
    return 0;

  const size_t bytes_read = input_.Read(data, data_size);
  VLOG(3) << "ReadCallback: Read " << bytes_read << " bytes from stream.";
  return bytes_read;
}

void DemuxerThread::OnResetRead() {
  std::unique_lock<Mutex> lock(mutex_);
  input_.SetBuffer(cur_data_, cur_size_);
}

void DemuxerThread::CallOnComplete(Status status) {
  if (on_complete_) {
    // on_complete must be invoked on the event thread.
    JsManagerImpl::Instance()->MainThread()->AddInternalTask(
        TaskPriority::Internal, "Append done",
        PlainCallbackTask(std::bind(on_complete_, status)));
    on_complete_ = std::function<void(Status)>();
  }
}

}  // namespace media
}  // namespace shaka
