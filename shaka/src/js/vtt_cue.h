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

#ifndef SHAKA_EMBEDDED_JS_VTT_CUE_H_
#define SHAKA_EMBEDDED_JS_VTT_CUE_H_

#include <memory>
#include <string>

#include "shaka/media/vtt_cue.h"
#include "shaka/variant.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/enum.h"

namespace shaka {
namespace js {

enum class AutoKeyword {
  Auto,
};

class VTTCue : public BackingObject {
  DECLARE_TYPE_INFO(VTTCue);

 public:
  VTTCue(double start_time, double end_time, const std::string& text);
  explicit VTTCue(std::shared_ptr<shaka::media::VTTCue> pub);

  static VTTCue* Create(double start, double end, const std::string& text) {
    return new VTTCue(start, end, text);
  }

  std::shared_ptr<shaka::media::VTTCue> GetPublic() const {
    return cue_;
  }

  std::string id() const;
  void SetId(const std::string& id);
  double start_time() const;
  void SetStartTime(double time);
  double end_time() const;
  void SetEndTime(double time);
  bool pause_on_exit() const;
  void SetPauseOnExit(bool pause);

  media::DirectionSetting vertical() const;
  void SetVertical(media::DirectionSetting setting);
  bool snap_to_lines() const;
  void SetSnapToLines(bool snap);
  media::LineAlignSetting line_align() const;
  void SetLineAlign(media::LineAlignSetting align);
  variant<double, AutoKeyword> line() const;
  void SetLine(variant<double, AutoKeyword> value);
  variant<double, AutoKeyword> position() const;
  void SetPosition(variant<double, AutoKeyword> value);
  media::PositionAlignSetting position_align() const;
  void SetPositionAlign(media::PositionAlignSetting align);
  double size() const;
  void SetSize(double size);
  media::AlignSetting align() const;
  void SetAlign(media::AlignSetting align);
  std::string text() const;
  void SetText(const std::string& text);

 private:
  std::shared_ptr<shaka::media::VTTCue> cue_;
};

class VTTCueFactory : public BackingObjectFactory<VTTCue> {
 public:
  VTTCueFactory();
};

}  // namespace js
}  // namespace shaka

DEFINE_ENUM_MAPPING(shaka::js, AutoKeyword) {
  AddMapping(Enum::Auto, "auto");
}

DEFINE_ENUM_MAPPING(shaka::media, DirectionSetting) {
  AddMapping(Enum::Horizontal, "");
  AddMapping(Enum::LeftToRight, "lr");
  AddMapping(Enum::RightToLeft, "rl");
}

DEFINE_ENUM_MAPPING(shaka::media, LineAlignSetting) {
  AddMapping(Enum::Start, "start");
  AddMapping(Enum::Center, "center");
  AddMapping(Enum::End, "end");
}

DEFINE_ENUM_MAPPING(shaka::media, PositionAlignSetting) {
  AddMapping(Enum::LineLeft, "line-left");
  AddMapping(Enum::Center, "center");
  AddMapping(Enum::LineRight, "line-right");
  AddMapping(Enum::Auto, "auto");
}

DEFINE_ENUM_MAPPING(shaka::media, AlignSetting) {
  AddMapping(Enum::Start, "start");
  AddMapping(Enum::Center, "center");
  AddMapping(Enum::End, "end");
  AddMapping(Enum::Left, "left");
  AddMapping(Enum::Right, "right");
}

#endif  // SHAKA_EMBEDDED_JS_VTT_CUE_H_
