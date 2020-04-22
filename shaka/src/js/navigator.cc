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

#include "src/js/navigator.h"

#include <utility>

#include "src/core/js_manager_impl.h"
#include "src/js/eme/search_registry.h"
#include "src/js/js_error.h"

namespace shaka {
namespace js {

Navigator::Navigator() {}
// \cond Doxygen_Skip
Navigator::~Navigator() {}
// \endcond Doxygen_Skip

Promise Navigator::RequestMediaKeySystemAccess(
    std::string key_system,
    std::vector<eme::MediaKeySystemConfiguration> configs) {
  // 1. If keySystem is the empty string, return a promise rejected with a
  // newly created TypeError.
  if (key_system.empty()) {
    return Promise::Rejected(
        JsError::TypeError("The keySystem parameter is empty."));
  }

  // 2. If supportedConfigurations is empty, return a promise rejected with a
  // newly created TypeError.
  if (configs.empty()) {
    return Promise::Rejected(
        JsError::TypeError("The configuration parameter is empty."));
  }

  // NA: 3. Let document be the calling context's Document.
  // NA: 4. Let origin be the origin of document.

  // 5. Let promise be a new Promise.
  Promise promise = Promise::PendingPromise();

  // 6. Run the following steps in parallel:
  JsManagerImpl::Instance()->MainThread()->AddInternalTask(
      TaskPriority::Internal, "search eme registry",
      eme::SearchRegistry(promise, std::move(key_system), std::move(configs)));

  // 7. Return promise.
  return promise;
}


NavigatorFactory::NavigatorFactory() {
  AddReadOnlyProperty("appName", &Navigator::app_name);
  AddReadOnlyProperty("appCodeName", &Navigator::app_code_name);
  AddReadOnlyProperty("appVersion", &Navigator::app_version);
  AddReadOnlyProperty("platform", &Navigator::platform);
  AddReadOnlyProperty("product", &Navigator::product);
  AddReadOnlyProperty("productSub", &Navigator::product_sub);
  AddReadOnlyProperty("vendor", &Navigator::vendor);
  AddReadOnlyProperty("vendorSub", &Navigator::vendor_sub);
  AddReadOnlyProperty("userAgent", &Navigator::user_agent);

  AddMemberFunction("requestMediaKeySystemAccess",
                    &Navigator::RequestMediaKeySystemAccess);
}
NavigatorFactory::~NavigatorFactory() {}

}  // namespace js
}  // namespace shaka
