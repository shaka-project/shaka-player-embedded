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

#ifndef SHAKA_EMBEDDED_JS_EME_IMPLEMENTATION_HELPER_IMPL_H_
#define SHAKA_EMBEDDED_JS_EME_IMPLEMENTATION_HELPER_IMPL_H_

#include <string>
#include <unordered_map>

#include "shaka/eme/implementation_helper.h"
#include "src/debug/mutex.h"
#include "src/js/eme/media_key_system_configuration.h"

namespace shaka {
namespace js {
namespace eme {

class MediaKeys;

/**
 * The implementation of the ImplementationHelper type.
 */
class ImplementationHelperImpl final : public ImplementationHelper {
 public:
  ImplementationHelperImpl(const std::string& key_system,
                           MediaKeys* media_keys);
  ~ImplementationHelperImpl() override;

  std::string DataPathPrefix() const override;

  void OnMessage(const std::string& session_id,
                 MediaKeyMessageType message_type, const uint8_t* data,
                 size_t data_size) const override;
  void OnKeyStatusChange(const std::string& session_id) const override;

 private:
  mutable Mutex mutex_;
  const std::string key_system_;
  MediaKeys* media_keys_;
};

}  // namespace eme
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_EME_IMPLEMENTATION_HELPER_IMPL_H_
