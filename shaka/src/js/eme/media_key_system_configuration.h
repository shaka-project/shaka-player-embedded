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

#ifndef SHAKA_EMBEDDED_JS_EME_MEDIA_KEY_SYSTEM_CONFIGURATION_H_
#define SHAKA_EMBEDDED_JS_EME_MEDIA_KEY_SYSTEM_CONFIGURATION_H_

#include <string>
#include <vector>

#include "shaka/eme/configuration.h"

#include "src/mapping/enum.h"
#include "src/mapping/struct.h"

namespace shaka {
namespace js {
namespace eme {

// Alias the names in shaka::eme to shaka::js::eme for convenience.
using namespace shaka::eme;  // NOLINT

struct MediaKeySystemMediaCapability : public Struct {
  static std::string name() {
    return "MediaKeySystemMediaCapability";
  }

  ADD_DICT_FIELD(std::string, contentType);
  ADD_DICT_FIELD(std::string, robustness);
};

struct MediaKeySystemConfiguration : public Struct {
  static std::string name() {
    return "MediaKeySystemConfiguration";
  }

  MediaKeySystemConfiguration();
  MediaKeySystemConfiguration(const MediaKeySystemConfiguration&);
  MediaKeySystemConfiguration(MediaKeySystemConfiguration&&);
  ~MediaKeySystemConfiguration() override;

  ADD_DICT_FIELD(std::string, label);
  ADD_DICT_FIELD(std::vector<MediaKeyInitDataType>, initDataTypes);
  ADD_DICT_FIELD(std::vector<MediaKeySystemMediaCapability>, audioCapabilities);
  ADD_DICT_FIELD(std::vector<MediaKeySystemMediaCapability>, videoCapabilities);
  ADD_DICT_FIELD(MediaKeysRequirement, distinctiveIdentifier);
  ADD_DICT_FIELD(MediaKeysRequirement, persistentState);
  ADD_DICT_FIELD(std::vector<MediaKeySessionType>, sessionTypes);
};

inline MediaKeySystemConfiguration::MediaKeySystemConfiguration() {}
inline MediaKeySystemConfiguration::~MediaKeySystemConfiguration() {}
inline MediaKeySystemConfiguration::MediaKeySystemConfiguration(
    const MediaKeySystemConfiguration&) = default;
inline MediaKeySystemConfiguration::MediaKeySystemConfiguration(
    MediaKeySystemConfiguration&&) = default;

}  // namespace eme
}  // namespace js
}  // namespace shaka

// Enums defined in <shaka/eme/configuration.h>
DEFINE_ENUM_MAPPING(shaka::eme, MediaKeysRequirement) {
  AddMapping(Enum::Required, "required");
  AddMapping(Enum::Optional, "optional");
  AddMapping(Enum::NotAllowed, "not-allowed");
}

DEFINE_ENUM_MAPPING(shaka::eme, MediaKeySessionType) {
  AddMapping(Enum::Temporary, "temporary");
  AddMapping(Enum::PersistentLicense, "persistent-license");
}

DEFINE_ENUM_MAPPING(shaka::eme, MediaKeyInitDataType) {
  AddMapping(Enum::Cenc, "cenc");
  AddMapping(Enum::KeyIds, "keyids");
  AddMapping(Enum::WebM, "webm");
}

DEFINE_ENUM_MAPPING(shaka::eme, MediaKeyMessageType) {
  AddMapping(Enum::LicenseRequest, "license-request");
  AddMapping(Enum::LicenseRenewal, "license-renewal");
  AddMapping(Enum::LicenseRelease, "license-release");
  AddMapping(Enum::IndividualizationRequest, "individualization-request");
}

DEFINE_ENUM_MAPPING(shaka::eme, MediaKeyStatus) {
  AddMapping(Enum::Usable, "usable");
  AddMapping(Enum::Expired, "expired");
  AddMapping(Enum::Released, "released");
  AddMapping(Enum::OutputRestricted, "output-restricted");
  AddMapping(Enum::OutputDownscaled, "output-downscaled");
  AddMapping(Enum::StatusPending, "status-pending");
  AddMapping(Enum::InternalError, "internal-error");
}

#endif  // SHAKA_EMBEDDED_JS_EME_MEDIA_KEY_SYSTEM_CONFIGURATION_H_
