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

#include "src/js/eme/media_key_system_access.h"

#include <utility>

#include "src/js/eme/media_keys.h"
#include "src/js/js_error.h"

namespace shaka {
namespace js {
namespace eme {

MediaKeySystemAccess::MediaKeySystemAccess(const std::string& key_system,
                                           MediaKeySystemConfiguration config,
                                           ImplementationFactory* factory)
    : key_system(key_system), config_(std::move(config)), factory_(factory) {}

// \cond Doxygen_Skip
MediaKeySystemAccess::~MediaKeySystemAccess() {}
// \endcond Doxygen_Skip

MediaKeySystemConfiguration MediaKeySystemAccess::GetConfiguration() const {
  return config_;
}

Promise MediaKeySystemAccess::CreateMediaKeys() {
  RefPtr<MediaKeys> media_keys = new MediaKeys(factory_, key_system, config_);
  if (!media_keys->valid()) {
    return Promise::Rejected(
        JsError::DOMException(UnknownError, "Unable to create CDM instance."));
  }

  LocalVar<JsValue> value(media_keys->JsThis());
  return Promise::Resolved(value);
}


MediaKeySystemAccessFactory::MediaKeySystemAccessFactory() {
  AddMemberFunction("getConfiguration",
                    &MediaKeySystemAccess::GetConfiguration);
  AddMemberFunction("createMediaKeys", &MediaKeySystemAccess::CreateMediaKeys);

  AddReadOnlyProperty("keySystem", &MediaKeySystemAccess::key_system);
}

}  // namespace eme
}  // namespace js
}  // namespace shaka
