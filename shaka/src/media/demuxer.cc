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

#include "shaka/media/demuxer.h"

#include <glog/logging.h>

#include <atomic>

#ifdef HAS_DEMUXER
#  include "src/media/ffmpeg/ffmpeg_demuxer.h"
#endif
#include "src/util/macros.h"

namespace shaka {
namespace media {

namespace {

std::atomic<const DemuxerFactory*> demuxer_factory_{nullptr};

}  // namespace

// \cond Doxygen_Skip
Demuxer::Demuxer() {}
Demuxer::~Demuxer() {}

Demuxer::Client::Client() {}
Demuxer::Client::~Client() {}
// \endcond Doxygen_Skip

bool Demuxer::SwitchType(const std::string& mime_type) {
  return false;
}


// \cond Doxygen_Skip
DemuxerFactory::DemuxerFactory() {}
DemuxerFactory::~DemuxerFactory() {}
// \endcond Doxygen_Skip

const DemuxerFactory* DemuxerFactory::GetFactory() {
  const DemuxerFactory* ret = demuxer_factory_.load(std::memory_order_relaxed);
  if (ret)
    return ret;

#ifdef HAS_DEMUXER
  static ffmpeg::FFmpegDemuxerFactory* factory =
      new ffmpeg::FFmpegDemuxerFactory;
  return factory;
#else
  return nullptr;
#endif
}

void DemuxerFactory::SetFactory(const DemuxerFactory* func) {
  demuxer_factory_.store(func, std::memory_order_relaxed);
}

bool DemuxerFactory::CanSwitchType(const std::string& old_mime_type,
                                   const std::string& new_mime_type) const {
  return false;
}

}  // namespace media
}  // namespace shaka
