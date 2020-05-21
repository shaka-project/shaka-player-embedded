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

#ifndef SHAKA_EMBEDDED_JS_EME_MEDIA_KEY_SYSTEM_ACCESS_H_
#define SHAKA_EMBEDDED_JS_EME_MEDIA_KEY_SYSTEM_ACCESS_H_

#include <memory>
#include <string>

#include "shaka/eme/implementation_factory.h"
#include "src/js/eme/media_key_system_configuration.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/promise.h"

namespace shaka {
namespace js {
namespace eme {

class MediaKeySystemAccess : public BackingObject {
  DECLARE_TYPE_INFO(MediaKeySystemAccess);

 public:
  MediaKeySystemAccess(const std::string& key_system,
                       MediaKeySystemConfiguration config,
                       std::shared_ptr<ImplementationFactory> factory);

  MediaKeySystemConfiguration GetConfiguration() const;
  Promise CreateMediaKeys();

  const std::string key_system;

 private:
  const MediaKeySystemConfiguration config_;
  std::shared_ptr<ImplementationFactory> const factory_;
};

class MediaKeySystemAccessFactory
    : public BackingObjectFactory<MediaKeySystemAccess> {
 public:
  MediaKeySystemAccessFactory();
};

}  // namespace eme
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_EME_MEDIA_KEY_SYSTEM_ACCESS_H_
