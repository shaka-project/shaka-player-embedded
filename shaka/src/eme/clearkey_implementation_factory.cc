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

#include "src/eme/clearkey_implementation_factory.h"

#include "src/eme/clearkey_implementation.h"

namespace shaka {
namespace eme {

bool ClearKeyImplementationFactory::SupportsSessionType(
    MediaKeySessionType type) const {
  return type == MediaKeySessionType::Temporary;
}

bool ClearKeyImplementationFactory::SupportsInitDataType(
    MediaKeyInitDataType type) const {
  return type == MediaKeyInitDataType::KeyIds ||
         type == MediaKeyInitDataType::Cenc;
}

bool ClearKeyImplementationFactory::SupportsAudioRobustness(
    const std::string& robustness) const {
  return robustness.empty();
}

bool ClearKeyImplementationFactory::SupportsVideoRobustness(
    const std::string& robustness) const {
  return robustness.empty();
}

MediaKeysRequirement ClearKeyImplementationFactory::DistinctiveIdentifier()
    const {
  return MediaKeysRequirement::NotAllowed;
}

MediaKeysRequirement ClearKeyImplementationFactory::PersistentState() const {
  return MediaKeysRequirement::NotAllowed;
}

std::shared_ptr<Implementation>
ClearKeyImplementationFactory::CreateImplementation(
    ImplementationHelper* helper,
    MediaKeysRequirement /* distinctive_identifier */,
    MediaKeysRequirement /* persistent_state */,
    const std::vector<std::string>& /* audio_robustness */,
    const std::vector<std::string>& /* video_robustness */) {
  return std::shared_ptr<Implementation>{new ClearKeyImplementation(helper)};
}

}  // namespace eme
}  // namespace shaka
