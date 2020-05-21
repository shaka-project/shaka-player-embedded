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

#ifndef SHAKA_EMBEDDED_JS_EME_MEDIA_KEYS_H_
#define SHAKA_EMBEDDED_JS_EME_MEDIA_KEYS_H_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "shaka/eme/configuration.h"
#include "shaka/eme/implementation.h"
#include "shaka/eme/implementation_factory.h"
#include "shaka/optional.h"

#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/js/eme/implementation_helper_impl.h"
#include "src/js/eme/media_key_system_configuration.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/byte_buffer.h"
#include "src/mapping/exception_or.h"
#include "src/mapping/promise.h"

namespace shaka {
namespace js {
namespace eme {

class MediaKeySession;

class MediaKeys : public BackingObject {
  DECLARE_TYPE_INFO(MediaKeys);

 public:
  MediaKeys(std::shared_ptr<ImplementationFactory> factory,
            const std::string& key_system,
            const MediaKeySystemConfiguration& config);

  void Trace(memory::HeapTracer* tracer) const override;

  bool valid() const;

  ExceptionOr<RefPtr<MediaKeySession>> CreateSession(
      optional<MediaKeySessionType> session_type);
  Promise SetServerCertificate(ByteBuffer cert);

  RefPtr<MediaKeySession> GetSession(const std::string& session_id);
  Implementation* GetCdm() const {
    return implementation_.get();
  }

  const std::string key_system;

 private:
  mutable std::mutex mutex_;
  // TODO: These should be weak pointers.
  std::vector<Member<MediaKeySession>> sessions_;
  ImplementationHelperImpl helper_;
  std::shared_ptr<ImplementationFactory> factory_;
  std::shared_ptr<Implementation> implementation_;
};

class MediaKeysFactory : public BackingObjectFactory<MediaKeys> {
 public:
  MediaKeysFactory();
};

}  // namespace eme
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_EME_MEDIA_KEYS_H_
