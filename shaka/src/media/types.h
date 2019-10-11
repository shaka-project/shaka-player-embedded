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

#ifndef SHAKA_EMBEDDED_MEDIA_TYPES_H_
#define SHAKA_EMBEDDED_MEDIA_TYPES_H_

#include <functional>  // For std::hash
#include <iostream>
#include <string>
#include <type_traits>

#include "src/mapping/struct.h"
#include "src/util/macros.h"

namespace shaka {
namespace media {

// enum class Status
#define DEFINE_ENUM_(DEFINE)                                                  \
  /** The operation succeeded. */                                             \
  DEFINE(Success)                                                             \
                                                                              \
  /**                                                                         \
   * The specified media stack (i.e. MediaSource) has been detached and       \
   * destroyed.                                                               \
   */                                                                         \
  DEFINE(Detached)                                                            \
                                                                              \
  /**                                                                         \
   * FFmpeg hit the end of its internal stream.  This is expected to happen   \
   * during shutdown, but is an internal error otherwise.                     \
   */                                                                         \
  DEFINE(EndOfStream)                                                         \
                                                                              \
  /** There is no source of the specified type. */                            \
  DEFINE(QuotaExceeded)                                                       \
                                                                              \
  /** The system wasn't able to allocate the required memory. */              \
  DEFINE(OutOfMemory)                                                         \
                                                                              \
  /** The specified action is not supported (e.g. unknown MIME type). */      \
  DEFINE(NotSupported)                                                        \
                                                                              \
  /**                                                                         \
   * The specified action is not allowed (e.g. adding a second video source). \
   */                                                                         \
  DEFINE(NotAllowed)                                                          \
                                                                              \
  /** An unknown error occurred; see log for system codes. */                 \
  DEFINE(UnknownError)                                                        \
                                                                              \
  /* ---- Demuxer Errors ---- */                                              \
  /**                                                                         \
   * We were unable to open the demuxer.  This usually happens because of     \
   * invalid input or a missing initialization segment.                       \
   */                                                                         \
  DEFINE(CannotOpenDemuxer)                                                   \
                                                                              \
  /** The input stream didn't have any elementary streams. */                 \
  DEFINE(NoStreamsFound)                                                      \
                                                                              \
  /**                                                                         \
   * The input stream contained multiplexed content, which isn't supported.   \
   */                                                                         \
  DEFINE(MultiplexedContentFound)                                             \
                                                                              \
  /** The container data was in an invalid format. */                         \
  DEFINE(InvalidContainerData)                                                \
                                                                              \
  /** ---- Decoder Errors ---- */                                             \
  /** The codec in the content didn't match the value initialized with. */    \
  DEFINE(DecoderMismatch)                                                     \
                                                                              \
  /** Unable to initialize the decoder. */                                    \
  DEFINE(DecoderFailedInit)                                                   \
                                                                              \
  /** There was an error in the codec data. */                                \
  DEFINE(InvalidCodecData)                                                    \
                                                                              \
  /**                                                                         \
   * The decryption key for the frame wasn't found.  This error isn't fatal;  \
   * once the CDM gets the required key the decoder can continue.             \
   */                                                                         \
  DEFINE(KeyNotFound)

DEFINE_ENUM_AND_TO_STRING(Status, DEFINE_ENUM_);
#undef DEFINE_ENUM_


// enum class SourceType
#define DEFINE_ENUM_(DEFINE) \
  DEFINE(Unknown)            \
  DEFINE(Audio)              \
  DEFINE(Video)

DEFINE_ENUM_AND_TO_STRING(SourceType, DEFINE_ENUM_);
#undef DEFINE_ENUM_


// enum class PipelineStatus
#define DEFINE_ENUM_(DEFINE)                                              \
  /** The pipeline is starting up. */                                     \
  DEFINE(Initializing)                                                    \
  /** The pipeline is playing media. */                                   \
  DEFINE(Playing)                                                         \
  /** The pipeline is paused (by user action). */                         \
  DEFINE(Paused)                                                          \
  /**                                                                     \
   * The pipeline is performing a seek and will play once done.  Note     \
   * that a seek is completed quickly, but we remain in this state until  \
   * we transition to Playing.  So this is similar to Stalled.            \
   */                                                                     \
  DEFINE(SeekingPlay)                                                     \
  /** Similar to SeekingPlay, but will remain paused. */                  \
  DEFINE(SeekingPause)                                                    \
  /**                                                                     \
   * The pipeline is stalled waiting for new content.  This only happens  \
   * when playing.  If the video is paused, it will be in Paused, even if \
   * there is no content.                                                 \
   */                                                                     \
  DEFINE(Stalled)                                                         \
  /** The video has ended and the pipeline is waiting for user action. */ \
  DEFINE(Ended)                                                           \
  /** There was an error that has stopped the pipeline. */                \
  DEFINE(Errored)

DEFINE_ENUM_AND_TO_STRING(PipelineStatus, DEFINE_ENUM_);
#undef DEFINE_ENUM_


enum MediaReadyState {
  HAVE_NOTHING = 0,
  HAVE_METADATA = 1,
  HAVE_CURRENT_DATA = 2,
  HAVE_FUTURE_DATA = 3,
  HAVE_ENOUGH_DATA = 4,
};


struct VideoPlaybackQuality : Struct {
  static std::string name() {
    return "VideoPlaybackQuality";
  }

  ADD_DICT_FIELD(creationTime, double);
  ADD_DICT_FIELD(totalVideoFrames, uint64_t);
  ADD_DICT_FIELD(droppedVideoFrames, uint64_t);
  ADD_DICT_FIELD(corruptedVideoFrames, uint64_t);
};

struct BufferedRange {
  BufferedRange() : start(0), end(0) {}
  BufferedRange(double start, double end) : start(start), end(end) {}

  bool operator==(const BufferedRange& other) const {
    return start == other.start && end == other.end;
  }
  bool operator!=(const BufferedRange& other) const {
    return !(*this == other);
  }

  double start;
  double end;
};
using BufferedRanges = std::vector<BufferedRange>;

std::ostream& operator<<(std::ostream& os, const BufferedRange& range);

/** @returns A string error message for the given status. */
std::string GetErrorString(Status status);

}  // namespace media
}  // namespace shaka

namespace std {

// To be used as keys in an unordered_{set,map} requires a definition of
// std::hash<T>.  C++14 adds a general definition for enums, but since we
// target C++11, we need to add our own.
template <>
struct hash<shaka::media::SourceType> {
  size_t operator()(shaka::media::SourceType type) const {
    return static_cast<size_t>(type);
  }
};

}  // namespace std

#endif  // SHAKA_EMBEDDED_MEDIA_TYPES_H_
