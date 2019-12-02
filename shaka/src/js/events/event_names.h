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

#ifndef SHAKA_EMBEDDED_JS_EVENTS_EVENT_NAMES_H_
#define SHAKA_EMBEDDED_JS_EVENTS_EVENT_NAMES_H_

#include "src/util/macros.h"

namespace shaka {
namespace js {

#define DEFINE_EVENTS_(DEFINE_EVENT)                     \
  DEFINE_EVENT(Abort, "abort")                           \
  DEFINE_EVENT(Error, "error")                           \
  DEFINE_EVENT(ReadyStateChange, "readystatechange")     \
  /** Media events */                                    \
  DEFINE_EVENT(CanPlay, "canplay")                       \
  DEFINE_EVENT(CanPlayThrough, "canplaythrough")         \
  DEFINE_EVENT(LoadedMetaData, "loadedmetadata")         \
  DEFINE_EVENT(LoadedData, "loadeddata")                 \
  DEFINE_EVENT(Waiting, "waiting")                       \
  DEFINE_EVENT(Emptied, "emptied")                       \
  DEFINE_EVENT(Play, "play")                             \
  DEFINE_EVENT(Playing, "playing")                       \
  DEFINE_EVENT(Pause, "pause")                           \
  DEFINE_EVENT(Seeked, "seeked")                         \
  DEFINE_EVENT(Seeking, "seeking")                       \
  DEFINE_EVENT(Ended, "ended")                           \
  DEFINE_EVENT(CueChange, "cuechange")                   \
  /* EME events. */                                      \
  DEFINE_EVENT(KeyStatusesChange, "keystatuseschange")   \
  DEFINE_EVENT(Message, "message")                       \
  DEFINE_EVENT(WaitingForKey, "waitingforkey")           \
  DEFINE_EVENT(Encrypted, "encrypted")                   \
  /* Progress tracking */                                \
  DEFINE_EVENT(Load, "load")                             \
  DEFINE_EVENT(LoadStart, "loadstart")                   \
  DEFINE_EVENT(LoadEnd, "loadend")                       \
  DEFINE_EVENT(Progress, "progress")                     \
  DEFINE_EVENT(Update, "update")                         \
  DEFINE_EVENT(UpdateStart, "updatestart")               \
  DEFINE_EVENT(UpdateEnd, "updateend")                   \
  /* XMLHttpRequest */                                   \
  DEFINE_EVENT(Timeout, "timeout")                       \
  /* MSE */                                              \
  DEFINE_EVENT(SourceOpen, "sourceopen")                 \
  DEFINE_EVENT(SourceEnded, "sourceended")               \
  DEFINE_EVENT(SourceClose, "sourceclose")               \
  DEFINE_EVENT(AddSourceBuffer, "addsourcebuffer")       \
  DEFINE_EVENT(RemoveSourceBuffer, "removesourcebuffer") \
  /* IndexedDB */                                        \
  DEFINE_EVENT(Complete, "complete")                     \
  DEFINE_EVENT(Success, "success")                       \
  DEFINE_EVENT(UpgradeNeeded, "upgradeneeded")           \
  DEFINE_EVENT(VersionChange, "versionchange")

DEFINE_ENUM_AND_TO_STRING_2(EventType, DEFINE_EVENTS_);
#undef DEFINE_EVENTS_

}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_EVENTS_EVENT_NAMES_H_
