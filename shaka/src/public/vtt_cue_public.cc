// Copyright 2018 Google LLC
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

#include "shaka/vtt_cue.h"

#include <cmath>

namespace shaka {

VTTCue::VTTCue(double start_time, double end_time, const std::string& text)
    : start_time_(start_time),
      end_time_(end_time),
      pause_on_exit_(false),
      vertical_(DirectionSetting::Horizontal),
      snap_to_lines_(true),
      line_(NAN),
      line_align_(LineAlignSetting::Start),
      position_(NAN),
      position_align_(PositionAlignSetting::Auto),
      size_(100),
      align_(AlignSetting::Center),
      text_(text) {}

VTTCue::VTTCue(const VTTCue& cue)
    : id_(cue.id_),
      start_time_(cue.start_time_),
      end_time_(cue.end_time_),
      pause_on_exit_(cue.pause_on_exit_),
      vertical_(cue.vertical_),
      snap_to_lines_(cue.snap_to_lines_),
      line_(cue.line_),
      line_align_(cue.line_align_),
      position_(cue.position_),
      position_align_(cue.position_align_),
      size_(cue.size_),
      align_(cue.align_),
      text_(cue.text_) {}

VTTCue::VTTCue(VTTCue&& cue)
    : id_(std::move(cue.id_)),
      start_time_(cue.start_time_),
      end_time_(cue.end_time_),
      pause_on_exit_(cue.pause_on_exit_),
      vertical_(cue.vertical_),
      snap_to_lines_(cue.snap_to_lines_),
      line_(cue.line_),
      line_align_(cue.line_align_),
      position_(cue.position_),
      position_align_(cue.position_align_),
      size_(cue.size_),
      align_(cue.align_),
      text_(std::move(cue.text_)) {}

VTTCue::~VTTCue() {}

VTTCue& VTTCue::operator=(const VTTCue& cue) {
  *this = VTTCue(cue);  // Use move-assignment and copy-constructor.
  return *this;
}

VTTCue& VTTCue::operator=(VTTCue&& cue) {
  id_ = std::move(cue.id_);
  start_time_ = cue.start_time_;
  end_time_ = cue.end_time_;
  pause_on_exit_ = cue.pause_on_exit_;
  vertical_ = cue.vertical_;
  snap_to_lines_ = cue.snap_to_lines_;
  line_ = cue.line_;
  line_align_ = cue.line_align_;
  position_ = cue.position_;
  position_align_ = cue.position_align_;
  size_ = cue.size_;
  align_ = cue.align_;
  text_ = std::move(cue.text_);
  return *this;
}

std::string VTTCue::id() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return id_;
}

void VTTCue::SetId(const std::string& id) {
  std::unique_lock<std::mutex> lock(mutex_);
  id_ = id;
}

double VTTCue::startTime() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return start_time_;
}

void VTTCue::SetStartTime(double time) {
  std::unique_lock<std::mutex> lock(mutex_);
  start_time_ = time;
}

double VTTCue::endTime() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return end_time_;
}

void VTTCue::SetEndTime(double time) {
  std::unique_lock<std::mutex> lock(mutex_);
  end_time_ = time;
}

bool VTTCue::pauseOnExit() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return pause_on_exit_;
}

void VTTCue::SetPauseOnExit(bool pause) {
  std::unique_lock<std::mutex> lock(mutex_);
  pause_on_exit_ = pause;
}

DirectionSetting VTTCue::vertical() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return vertical_;
}

void VTTCue::SetVertical(DirectionSetting setting) {
  std::unique_lock<std::mutex> lock(mutex_);
  vertical_ = setting;
}

bool VTTCue::snapToLines() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return snap_to_lines_;
}

void VTTCue::SetSnapToLines(bool snap) {
  std::unique_lock<std::mutex> lock(mutex_);
  snap_to_lines_ = snap;
}

LineAlignSetting VTTCue::lineAlign() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return line_align_;
}

void VTTCue::SetLineAlign(LineAlignSetting align) {
  std::unique_lock<std::mutex> lock(mutex_);
  line_align_ = align;
}

double VTTCue::line() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return line_;
}

void VTTCue::SetLine(double line) {
  std::unique_lock<std::mutex> lock(mutex_);
  line_ = line;
}

double VTTCue::position() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return position_;
}

void VTTCue::SetPosition(double position) {
  std::unique_lock<std::mutex> lock(mutex_);
  position_ = position;
}

PositionAlignSetting VTTCue::positionAlign() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return position_align_;
}

void VTTCue::SetPositionAlign(PositionAlignSetting align) {
  std::unique_lock<std::mutex> lock(mutex_);
  position_align_ = align;
}

double VTTCue::size() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return size_;
}

void VTTCue::SetSize(double size) {
  std::unique_lock<std::mutex> lock(mutex_);
  size_ = size;
}

AlignSetting VTTCue::align() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return align_;
}

void VTTCue::SetAlign(AlignSetting align) {
  std::unique_lock<std::mutex> lock(mutex_);
  align_ = align;
}

std::string VTTCue::text() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return text_;
}

void VTTCue::SetText(const std::string& text) {
  std::unique_lock<std::mutex> lock(mutex_);
  text_ = text;
}

}  // namespace shaka
