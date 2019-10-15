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

#include "shaka/media/streams.h"

namespace shaka {
namespace media {

class StreamBase::Impl {};

StreamBase::StreamBase(bool order_by_dts) {}
StreamBase::~StreamBase() {}

size_t StreamBase::CountFramesBetween(double start, double end) const {
  return 0;
}

std::vector<BufferedRange> StreamBase::GetBufferedRanges() const {
  return {};
}

size_t StreamBase::EstimateSize() const {
  return 0;
}

void StreamBase::Remove(double start, double end) {}

void StreamBase::Clear() {}

std::shared_ptr<BaseFrameNew> StreamBase::GetFrameInternal(
    double time, FrameLocation kind) const {
  return nullptr;
}

void StreamBase::AddFrameInternal(std::shared_ptr<BaseFrameNew> frame) {}

}  // namespace media
}  // namespace shaka
