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
    : cue_(new shaka::media::VTTCue(start_time, end_time, text)) {}

VTTCue::VTTCue(std::shared_ptr<shaka::media::VTTCue> pub) : cue_(pub) {}

VTTCue::~VTTCue() {}

std::string VTTCue::id() const {
  return cue_->id();
}

void VTTCue::SetId(const std::string& id) {
  cue_->SetId(id);
}

double VTTCue::start_time() const {
  return cue_->start_time();
}

void VTTCue::SetStartTime(double time) {
  cue_->SetStartTime(time);
}

double VTTCue::end_time() const {
  return cue_->end_time();
}

void VTTCue::SetEndTime(double time) {
  cue_->SetEndTime(time);
}

bool VTTCue::pause_on_exit() const {
  return cue_->pause_on_exit();
}

void VTTCue::SetPauseOnExit(bool pause) {
  cue_->SetPauseOnExit(pause);
}


media::DirectionSetting VTTCue::vertical() const {
  return cue_->vertical();
}

void VTTCue::SetVertical(media::DirectionSetting setting) {
  cue_->SetVertical(setting);
}

bool VTTCue::snap_to_lines() const {
  return cue_->snap_to_lines();
}

void VTTCue::SetSnapToLines(bool snap) {
  cue_->SetSnapToLines(snap);
}

media::LineAlignSetting VTTCue::line_align() const {
  return cue_->line_align();
}

void VTTCue::SetLineAlign(media::LineAlignSetting align) {
  cue_->SetLineAlign(align);
}

variant<double, AutoKeyword> VTTCue::line() const {
  const double ret = cue_->line();
  if (std::isnan(ret))
    return AutoKeyword::Auto;

  return ret;
}

void VTTCue::SetLine(variant<double, AutoKeyword> value) {
  if (holds_alternative<double>(value))
    cue_->SetLine(get<double>(value));
  else
    cue_->SetLine(NAN);
}

variant<double, AutoKeyword> VTTCue::position() const {
  const double ret = cue_->position();
  if (std::isnan(ret))
    return AutoKeyword::Auto;

  return ret;
}

void VTTCue::SetPosition(variant<double, AutoKeyword> value) {
  if (holds_alternative<double>(value))
    cue_->SetPosition(get<double>(value));
  else
    cue_->SetPosition(NAN);
}

media::PositionAlignSetting VTTCue::position_align() const {
  return cue_->position_align();
}

void VTTCue::SetPositionAlign(media::PositionAlignSetting align) {
  cue_->SetPositionAlign(align);
}

double VTTCue::size() const {
  return cue_->size();
}

void VTTCue::SetSize(double size) {
  cue_->SetSize(size);
}

media::AlignSetting VTTCue::align() const {
  return cue_->align();
}

void VTTCue::SetAlign(media::AlignSetting align) {
  cue_->SetAlign(align);
}

std::string VTTCue::text() const {
  return cue_->text();
}

void VTTCue::SetText(const std::string& text) {
  cue_->SetText(text);
}


VTTCueFactory::VTTCueFactory() {
  // TextTrackCue
  AddGenericProperty("id", &VTTCue::id, &VTTCue::SetId);
  AddGenericProperty("startTime", &VTTCue::start_time, &VTTCue::SetStartTime);
  AddGenericProperty("endTime", &VTTCue::end_time, &VTTCue::SetEndTime);
  AddGenericProperty("pauseOnExit", &VTTCue::pause_on_exit,
                     &VTTCue::SetPauseOnExit);

  // VTTCue
  AddGenericProperty("vertical", &VTTCue::vertical, &VTTCue::SetVertical);
  AddGenericProperty("snapToLines", &VTTCue::snap_to_lines,
                     &VTTCue::SetSnapToLines);
  AddGenericProperty("line", &VTTCue::line, &VTTCue::SetLine);
  AddGenericProperty("lineAlign", &VTTCue::line_align, &VTTCue::SetLineAlign);
  AddGenericProperty("position", &VTTCue::position, &VTTCue::SetPosition);
  AddGenericProperty("positionAlign", &VTTCue::position_align,
                     &VTTCue::SetPositionAlign);
  AddGenericProperty("size", &VTTCue::size, &VTTCue::SetSize);
  AddGenericProperty("align", &VTTCue::align, &VTTCue::SetAlign);
  AddGenericProperty("text", &VTTCue::text, &VTTCue::SetText);
}

}  // namespace js
}  // namespace shaka
