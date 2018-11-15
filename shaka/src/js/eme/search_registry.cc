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

#include "src/js/eme/search_registry.h"

#include <utility>

#include "shaka/eme/implementation.h"
#include "shaka/eme/implementation_factory.h"
#include "shaka/eme/implementation_registry.h"
#include "src/core/ref_ptr.h"
#include "src/js/eme/media_key_system_access.h"
#include "src/js/js_error.h"
#include "src/js/mse/media_source.h"


namespace shaka {
namespace js {
namespace eme {

namespace {

bool IsPersistentSessionType(MediaKeySessionType type) {
  return type == MediaKeySessionType::PersistentLicense;
}

bool SupportsContentType(const std::string& content_type) {
  media::SourceType unused_type;
  std::string container;
  std::string unused_codec;
  if (!ParseMimeAndCheckSupported(content_type, &unused_type, &container,
                                  &unused_codec)) {
    return false;
  }
  // FFmpeg demuxer only exposes encryption info for MP4 and WebM.
  if (container != "mp4" && container != "webm")
    return false;

  return true;
}

bool GetSupportedConfiguration(
    const ImplementationFactory* implementation,
    const MediaKeySystemConfiguration& candidate_config,
    MediaKeySystemConfiguration* supported_config) {
  // 1. Let accumulated configuration be a new MediaKeySystemConfiguration
  // dictionary.
  // Note: accumulated configuration == supported_config

  // 2. Set the label member of accumulated configuration to equal the label
  // member of candidate configuration.
  supported_config->label = candidate_config.label;

  // 3. If the initDataTypes member of candidate configuration is non-empty, run
  // the following steps:
  if (!candidate_config.initDataTypes.empty()) {
    // 1. Let supported types be an empty sequence of DOMStrings.
    supported_config->initDataTypes.clear();

    // 2. For each value in candidate configuration's initDataTypes member:
    for (auto& init_data_type : candidate_config.initDataTypes) {
      // 1. Let initDataType be the value.
      // 2. If the implementation supports generating requests based on
      // initDataType, add initDataType to supported types. String comparison is
      // case-sensitive. The empty string is never supported.
      if (implementation->SupportsInitDataType(init_data_type))
        supported_config->initDataTypes.push_back(init_data_type);
    }

    // 3. If supported types is empty, return NotSupported.
    if (supported_config->initDataTypes.empty()) {
      VLOG(1) << "None of the init data types are supported";
      return false;
    }
  }

  // 4. Let distinctive identifier requirement be the value of candidate
  // configuration's distinctiveIdentifier member.
  MediaKeysRequirement distinctive_identifier =
      candidate_config.distinctiveIdentifier;
  // NA: 5. If distinctive identifier requirement is "optional" and Distinctive
  // Identifiers are not allowed according to restrictions, set distinctive
  // identifier requirement to "not-allowed".
  // 6. Follow the steps for distinctive identifier requirement from the
  // following list:
  switch (distinctive_identifier) {
    case MediaKeysRequirement::Required:
      if (implementation->DistinctiveIdentifier() ==
          MediaKeysRequirement::NotAllowed) {
        VLOG(1) << "Distinctive identifier is required by app, but unsupported "
                   "by implementation";
        return false;
      }
      break;
    case MediaKeysRequirement::Optional:
      break;
    case MediaKeysRequirement::NotAllowed:
      if (implementation->DistinctiveIdentifier() ==
          MediaKeysRequirement::Required) {
        VLOG(1) << "Distinctive identifier is required by implementation, but "
                   "app doesn't allow it";
        return false;
      }
      break;
  }

  // 7. Set the distinctiveIdentifier member of accumulated configuration to
  // equal distinctive identifier requirement.
  supported_config->distinctiveIdentifier = distinctive_identifier;

  // 8. Let persistent state requirement be equal to the value of candidate
  // configuration's persistentState member.
  MediaKeysRequirement persistent_state = candidate_config.persistentState;
  // NA: 9. If persistent state requirement is "optional" and persisting state
  // is not allowed according to restrictions, set persistent state requirement
  // to "not-allowed".
  // 10. Follow the steps for persistent state requirement from the following
  // list:
  switch (persistent_state) {
    case MediaKeysRequirement::Required:
      if (implementation->PersistentState() ==
          MediaKeysRequirement::NotAllowed) {
        VLOG(1) << "Persistent state is required by app, but unsupported by "
                   "implementation";
        return false;
      }
      break;
    case MediaKeysRequirement::Optional:
      break;
    case MediaKeysRequirement::NotAllowed:
      if (implementation->PersistentState() == MediaKeysRequirement::Required) {
        VLOG(1) << "Persistent state is required by implementation, but app "
                   "doesn't allow it";
        return false;
      }
      break;
  }

  // 11. Set the persistentState member of accumulated configuration to equal
  // the value of persistent state requirement.
  supported_config->persistentState = persistent_state;

  // 12. Follow the steps for the first matching condition from the following
  // list:
  std::vector<MediaKeySessionType> session_types =
      candidate_config.sessionTypes;
  if (session_types.empty())
    session_types.push_back(MediaKeySessionType::Temporary);

  // 13. For each value in session types.
  for (auto& session_type : session_types) {
    // 1. Let session type be the value.

    // 2. If accumulated configuration's persistentState value is "not-allowed"
    // and the Is persistent session type? algorithm returns true for session
    // type return NotSupported.
    if (supported_config->persistentState == MediaKeysRequirement::NotAllowed &&
        IsPersistentSessionType(session_type)) {
      VLOG(1) << "Request for persistent session but persistentState is "
                 "'not-allowed'";
      return false;
    }

    // 3. If the implementation does not support session type in combination
    // with accumulated configuration and restrictions for other reasons, return
    // NotSupported.
    if (!implementation->SupportsSessionType(session_type)) {
      VLOG(1) << "Implementation doesn't support session type";
      return false;
    }

    // 4. If accumulated configuration's persistentState value is "optional" and
    // the result of running the Is persistent session type? algorithm on
    // session type is true, change accumulated configuration's persistentState
    // value to "required".
    if (IsPersistentSessionType(session_type)) {
      // The value NotAllowed was handled above, so the value is either
      // Optional or already Required.
      supported_config->persistentState = MediaKeysRequirement::Required;
    }
  }

  // 14. Set the sessionTypes member of accumulated configuration to session
  // types.
  supported_config->sessionTypes = session_types;

  // 15. If the videoCapabilities and audioCapabilities members in candidate
  // configuration are both empty, return NotSupported.
  if (candidate_config.audioCapabilities.empty() &&
      candidate_config.videoCapabilities.empty()) {
    VLOG(1) << "No audio/video capabilities given";
    return false;
  }

  // 16. If the videoCapabilities member in candidate configuration is non-empty
  if (!candidate_config.videoCapabilities.empty()) {
    for (auto& video_cap : candidate_config.videoCapabilities) {
      if (SupportsContentType(video_cap.contentType) &&
          implementation->SupportsVideoRobustness(video_cap.robustness)) {
        supported_config->videoCapabilities.push_back(video_cap);
      }
    }

    if (supported_config->videoCapabilities.empty()) {
      VLOG(1) << "None of the video capabilities are supported";
      return false;
    }
  }

  // 17. If the audioCapabilities member in candidate configuration is non-empty
  if (!candidate_config.audioCapabilities.empty()) {
    for (auto& audio_cap : candidate_config.audioCapabilities) {
      if (SupportsContentType(audio_cap.contentType) &&
          implementation->SupportsAudioRobustness(audio_cap.robustness)) {
        supported_config->audioCapabilities.push_back(audio_cap);
      }
    }

    if (supported_config->audioCapabilities.empty()) {
      VLOG(1) << "None of the audio capabilities are supported";
      return false;
    }
  }

  // 18. If accumulated configuration's distinctiveIdentifier value is
  // "optional", follow the steps for the first matching condition from the
  // following list:
  if (supported_config->distinctiveIdentifier ==
      MediaKeysRequirement::Optional) {
    if (implementation->DistinctiveIdentifier() ==
        MediaKeysRequirement::Required) {
      supported_config->distinctiveIdentifier = MediaKeysRequirement::Required;
    } else {
      supported_config->distinctiveIdentifier =
          MediaKeysRequirement::NotAllowed;
    }
  }

  // 19. If accumulated configuration's persistentState value is "optional",
  // follow the steps for the first matching condition from the following list:
  if (supported_config->persistentState == MediaKeysRequirement::Optional) {
    if (implementation->PersistentState() == MediaKeysRequirement::Required) {
      supported_config->persistentState = MediaKeysRequirement::Required;
    } else {
      supported_config->persistentState = MediaKeysRequirement::NotAllowed;
    }
  }

  // Ignore remaining steps since they pertain to consent.

  return true;
}

}  // namespace

SearchRegistry::SearchRegistry(Promise promise, std::string key_system,
                               std::vector<MediaKeySystemConfiguration> configs)
    : promise_(std::move(promise)),
      key_system_(std::move(key_system)),
      configs_(std::move(configs)) {}

SearchRegistry::~SearchRegistry() {}

SearchRegistry::SearchRegistry(SearchRegistry&&) = default;
SearchRegistry& SearchRegistry::operator=(SearchRegistry&&) = default;

void SearchRegistry::Trace(memory::HeapTracer* tracer) const {
  tracer->Trace(&promise_);
  tracer->Trace(&configs_);
}

void SearchRegistry::operator()() {
  // 1. If keySystem is not one of the Key Systems supported by the user agent,
  // reject promise with a NotSupportedError. String comparison is
  // case-sensitive.
  // 2. Let implementation be the implementation of keySystem.
  auto* implementation = ImplementationRegistry::GetImplementation(key_system_);
  if (!implementation) {
    VLOG(1) << "No implementation found for: " << key_system_;
    promise_.RejectWith(JsError::DOMException(
        NotSupportedError, "Key system " + key_system_ + " is not supported."));
    return;
  }

  // 3. For each value in supportedConfigurations:
  for (auto& candidate_config : configs_) {
    // 1. Let candidate configuration be the value.
    // 2. Let supported configuration be the result of executing the Get
    // Supported Configuration algorithm on implementation, candidate
    // configuration, and origin.
    // 3. If supported configuration is not NotSupported, run the following
    // steps:
    MediaKeySystemConfiguration supported_config;
    if (GetSupportedConfiguration(implementation, candidate_config,
                                  &supported_config)) {
      // 1. Let access be a new MediaKeySystemAccess object, and initialize it
      // as follows:
      RefPtr<MediaKeySystemAccess> access(new MediaKeySystemAccess(
          key_system_, supported_config, implementation));

      // 2. Resolve promise with access and abort the parallel steps of this
      // algorithm.
      LocalVar<JsValue> value(ToJsValue(access));
      promise_.ResolveWith(value);
      return;
    }
  }

  // 4. Reject promise with NotSupportedError.
  promise_.RejectWith(JsError::DOMException(
      NotSupportedError, "None of the given configurations are supported."));
}

}  // namespace eme
}  // namespace js
}  // namespace shaka
