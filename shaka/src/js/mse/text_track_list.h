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

#ifndef SHAKA_EMBEDDED_JS_MSE_TEXT_TRACK_LIST_H_
#define SHAKA_EMBEDDED_JS_MSE_TEXT_TRACK_LIST_H_

#include <vector>

#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/js/events/event_target.h"
#include "src/mapping/backing_object_factory.h"

namespace shaka {
namespace js {
namespace mse {

class TextTrack;

class TextTrackList : public events::EventTarget {
  DECLARE_TYPE_INFO(TextTrackList);

 public:
  explicit TextTrackList(const std::vector<RefPtr<TextTrack>>& tracks);

  void Trace(memory::HeapTracer* tracer) const override;

  size_t length() const {
    return text_tracks_.size();
  }
  bool GetIndex(size_t i, RefPtr<TextTrack>* track) const;

 private:
  std::vector<Member<TextTrack>> text_tracks_;
};

class TextTrackListFactory
    : public BackingObjectFactory<TextTrackList, events::EventTarget> {
 public:
  TextTrackListFactory();
};

}  // namespace mse
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_MSE_TEXT_TRACK_LIST_H_
