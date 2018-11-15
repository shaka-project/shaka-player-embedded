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

#include "src/media/types.h"

#include "src/util/utils.h"

namespace shaka {
namespace media {

std::ostream& operator<<(std::ostream& os, const BufferedRange& range) {
  return os << util::StringPrintf("{ start: %.2f, end: %.2f }", range.start,
                                  range.end);
}

std::string GetErrorString(Status status) {
  switch (status) {
    case Status::Success:
      return "The operation succeeded";

    case Status::Detached:
      return "The MediaSource/SourceBuffer has been detached and destroyed";
    case Status::EndOfStream:
      return "INTERNAL BUG: Unexpected end of stream";
    case Status::QuotaExceeded:
      return "Attempted to append media that would exceed the allowed quota";
    case Status::OutOfMemory:
      return "The system wasn't able to allocate the required memory";
    case Status::NotSupported:
      return "The specified action is not supported";
    case Status::NotAllowed:
      return "The specified action is not allowed";
    case Status::UnknownError:
      return "An unknown error occurred; see log for system codes";

    case Status::CannotOpenDemuxer:
      return "Unable to initialize the demuxer";
    case Status::NoStreamsFound:
      return "The input stream didn't have any elementary streams";
    case Status::MultiplexedContentFound:
      return "The input stream contained multiplexed content";
    case Status::InvalidContainerData:
      return "The container data was in an invalid format";

    case Status::DecoderMismatch:
      return "The codec in the content didn't match the value initialized with";
    case Status::DecoderFailedInit:
      return "Unable to initialize the decoder";
    case Status::InvalidCodecData:
      return "The codec data was in an invalid format";
    case Status::KeyNotFound:
      return "The required encryption key was not found";
  }
}

}  // namespace media
}  // namespace shaka
