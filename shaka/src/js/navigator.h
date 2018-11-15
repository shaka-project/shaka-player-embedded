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

#ifndef SHAKA_EMBEDDED_JS_NAVIGATOR_H_
#define SHAKA_EMBEDDED_JS_NAVIGATOR_H_

#include <string>
#include <vector>

#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/promise.h"
#include "src/js/eme/media_key_system_configuration.h"

// #define PLATFORM      // Defined by BUILD.gn
#define APP_NAME        "Netscape"
#define APP_CODE_NAME   "Mozilla"
#define APP_VERSION     "5.0"
#define PRODUCT         "Gecko"
#define PRODUCT_SUB     "20030107"
#define VENDOR          "Shaka-Player-Embedded"
// TODO: Incorporate versioning into build process.
#define VENDOR_SUB      "v0.0.1"
#define USER_AGENT \
  APP_CODE_NAME "/" APP_VERSION " (" PLATFORM ") " VENDOR "/" VENDOR_SUB


namespace shaka {
namespace js {

class Navigator : public BackingObject {
  DECLARE_TYPE_INFO(Navigator);

 public:
  Navigator();

  const std::string app_name = APP_NAME;
  const std::string app_code_name = APP_CODE_NAME;
  const std::string app_version = APP_VERSION;
  const std::string platform = PLATFORM;
  const std::string product = PRODUCT;
  const std::string product_sub = PRODUCT_SUB;
  const std::string vendor = VENDOR;
  const std::string vendor_sub = VENDOR_SUB;
  const std::string user_agent = USER_AGENT;

  Promise RequestMediaKeySystemAccess(
      std::string key_system,
      std::vector<eme::MediaKeySystemConfiguration> configs);
};

class NavigatorFactory : public BackingObjectFactory<Navigator> {
 public:
  NavigatorFactory();
  ~NavigatorFactory() override;
};

}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_NAVIGATOR_H_
