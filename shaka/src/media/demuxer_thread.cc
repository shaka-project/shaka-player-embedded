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
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/core/js_manager_impl.h"
#include "src/media/media_utils.h"

namespace shaka {
namespace media {

namespace {

std::string ShortContainerName(const std::string& mime) {
  std::string subtype;
  if (!ParseMimeType(mime, nullptr, &subtype, nullptr))
    return "";

  DCHECK_LT(subtype.size(), 8u) << "Container needs a short name";
  return subtype.substr(0, 8);
}

}  // namespace

DemuxerThread::DemuxerThread(const std::string& mime, Demuxer::Client* client,
                             ElementaryStream* stream)
    : mutex_("DemuxerThread"),
      new_data_("New demuxed data"),
      client_(client),
      mime_(mime),
      shutdown_(false),
      data_(nullptr),
      data_size_(0),
      timestamp_offset_(0),
      window_start_(0),
      window_end_(HUGE_VAL),
      need_key_frame_(true),
      stream_(stream),
      thread_(ShortContainerName(mime) + " demuxer",
              std::bind(&DemuxerThread::ThreadMain, this)) {}

DemuxerThread::~DemuxerThread() {
  if (thread_.joinable())
    Stop();
}

void DemuxerThread::Stop() {
  shutdown_ = true;
  new_data_.SignalAllIfNotSet();
  thread_.join();
}

void DemuxerThread::AppendData(double timestamp_offset, double window_start,
                               double window_end, const uint8_t* data,
                               size_t data_size,
                               std::function<void(bool)> on_complete) {
  DCHECK(data);
  DCHECK_GT(data_size, 0u);

  std::unique_lock<Mutex> lock(mutex_);
  data_ = data;
  data_size_ = data_size;
  timestamp_offset_ = timestamp_offset;
  window_start_ = window_start;
  window_end_ = window_end;
  on_complete_ = std::move(on_complete);

  new_data_.SignalAll();
}

void DemuxerThread::ThreadMain() {
  using namespace std::placeholders;  // NOLINT
  auto* factory = DemuxerFactory::GetFactory();
  if (factory)
    demuxer_ = factory->Create(mime_, client_);
  if (!demuxer_) {
    // If we get an error before we append the first segment, then we won't have
    // a callback yet, so we have nowhere to send the error to.  Wait until we
    // get the first segment.
    std::unique_lock<Mutex> lock(mutex_);
    if (!on_complete_ && !shutdown_) {
      new_data_.ResetAndWaitWhileUnlocked(lock);
    }
    CallOnComplete(false);
    return;
  }

  while (!shutdown_) {
    std::unique_lock<Mutex> lock(mutex_);
    std::vector<std::shared_ptr<EncodedFrame>> frames;
    if (!demuxer_->Demux(timestamp_offset_, data_, data_size_, &frames)) {
      CallOnComplete(false);
      break;
    }

    for (auto& frame : frames) {
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
      stream_->AddFrame(frame);
    }
    CallOnComplete(true);
    new_data_.ResetAndWaitWhileUnlocked(lock);
  }
}

void DemuxerThread::CallOnComplete(bool success) {
  if (on_complete_) {
    // on_complete must be invoked on the event thread.
    JsManagerImpl::Instance()->MainThread()->AddInternalTask(
        TaskPriority::Internal, "Append done",
        std::bind(on_complete_, success));
    on_complete_ = std::function<void(bool)>();
  }
}

}  // namespace media
}  // namespace shaka
