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

#include <string>

#include "shaka/variant.h"
#include "shaka/vtt_cue.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/enum.h"

namespace shaka {
namespace js {

enum class AutoKeyword {
  Auto,
};

class VTTCue : public BackingObject, public shaka::VTTCue {
  DECLARE_TYPE_INFO(VTTCue);

 public:
  VTTCue(double start_time, double end_time, const std::string& text);
  explicit VTTCue(const shaka::VTTCue& pub);

  static VTTCue* Create(double start, double end, const std::string& text) {
    return new VTTCue(start, end, text);
  }

  variant<double, AutoKeyword> PositionJs() const;
  void SetPositionJs(variant<double, AutoKeyword> value);
  variant<double, AutoKeyword> LineJs() const;
  void SetLineJs(variant<double, AutoKeyword> value);
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

DEFINE_ENUM_MAPPING(shaka, DirectionSetting) {
  AddMapping(Enum::Horizontal, "");
  AddMapping(Enum::LeftToRight, "lr");
  AddMapping(Enum::RightToLeft, "rl");
}

DEFINE_ENUM_MAPPING(shaka, LineAlignSetting) {
  AddMapping(Enum::Start, "start");
  AddMapping(Enum::Center, "center");
  AddMapping(Enum::End, "end");
}

DEFINE_ENUM_MAPPING(shaka, PositionAlignSetting) {
  AddMapping(Enum::LineLeft, "line-left");
  AddMapping(Enum::Center, "center");
  AddMapping(Enum::LineRight, "line-right");
  AddMapping(Enum::Auto, "auto");
}

DEFINE_ENUM_MAPPING(shaka, AlignSetting) {
  AddMapping(Enum::Start, "start");
  AddMapping(Enum::Center, "center");
  AddMapping(Enum::End, "end");
  AddMapping(Enum::Left, "left");
  AddMapping(Enum::Right, "right");
}

#endif  // SHAKA_EMBEDDED_JS_VTT_CUE_H_
