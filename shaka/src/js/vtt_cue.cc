// Copyright 2016 Google LLC
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

#include "src/js/vtt_cue.h"

#include <cmath>

namespace shaka {
namespace js {

VTTCue::VTTCue(double start_time, double end_time, const std::string& text)
    : shaka::VTTCue(start_time, end_time, text) {}

VTTCue::VTTCue(const shaka::VTTCue& pub) : shaka::VTTCue(pub) {}

VTTCue::~VTTCue() {}

variant<double, AutoKeyword> VTTCue::PositionJs() const {
  const double ret = position();
  if (std::isnan(ret))
    return AutoKeyword::Auto;

  return ret;
}

void VTTCue::SetPositionJs(variant<double, AutoKeyword> value) {
  if (holds_alternative<double>(value))
    SetPosition(get<double>(value));
  else
    SetPosition(NAN);
}

variant<double, AutoKeyword> VTTCue::LineJs() const {
  const double ret = line();
  if (std::isnan(ret))
    return AutoKeyword::Auto;

  return ret;
}

void VTTCue::SetLineJs(variant<double, AutoKeyword> value) {
  if (holds_alternative<double>(value))
    SetLine(get<double>(value));
  else
    SetLine(NAN);
}


VTTCueFactory::VTTCueFactory() {
  // TextTrackCue
  AddGenericProperty<VTTCue>("id", &VTTCue::id, &VTTCue::SetId);
  AddGenericProperty<VTTCue>("startTime", &VTTCue::startTime,
                             &VTTCue::SetStartTime);
  AddGenericProperty<VTTCue>("endTime", &VTTCue::endTime, &VTTCue::SetEndTime);
  AddGenericProperty<VTTCue>("pauseOnExit", &VTTCue::pauseOnExit,
                             &VTTCue::SetPauseOnExit);

  // VTTCue
  AddGenericProperty<VTTCue>("vertical", &VTTCue::vertical,
                             &VTTCue::SetVertical);
  AddGenericProperty<VTTCue>("snapToLines", &VTTCue::snapToLines,
                             &VTTCue::SetSnapToLines);
  AddGenericProperty<VTTCue>("line", &VTTCue::LineJs, &VTTCue::SetLineJs);
  AddGenericProperty<VTTCue>("lineAlign", &VTTCue::lineAlign,
                             &VTTCue::SetLineAlign);
  AddGenericProperty<VTTCue>("position", &VTTCue::PositionJs,
                             &VTTCue::SetPositionJs);
  AddGenericProperty<VTTCue>("positionAlign", &VTTCue::positionAlign,
                             &VTTCue::SetPositionAlign);
  AddGenericProperty<VTTCue>("size", &VTTCue::size, &VTTCue::SetSize);
  AddGenericProperty<VTTCue>("align", &VTTCue::align, &VTTCue::SetAlign);
  AddGenericProperty<VTTCue>("text", &VTTCue::text, &VTTCue::SetText);
}

}  // namespace js
}  // namespace shaka
