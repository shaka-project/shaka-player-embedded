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

#ifndef SHAKA_EMBEDDED_JS_EME_MEDIA_KEY_SESSION_H_
#define SHAKA_EMBEDDED_JS_EME_MEDIA_KEY_SESSION_H_

#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "shaka/eme/configuration.h"
#include "shaka/eme/implementation.h"
#include "shaka/eme/implementation_factory.h"

#include "src/debug/mutex.h"
#include "src/js/eme/media_key_system_configuration.h"
#include "src/js/events/event_target.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/byte_buffer.h"
#include "src/mapping/exception_or.h"
#include "src/mapping/promise.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace eme {

class MediaKeySession : public events::EventTarget {
  DECLARE_TYPE_INFO(MediaKeySession);

 public:
  MediaKeySession(MediaKeySessionType type,
                  std::shared_ptr<ImplementationFactory> factory,
                  std::shared_ptr<Implementation> implementation);

  void Trace(memory::HeapTracer* tracer) const override;

  std::string SessionId() const;

  Promise closed;

  Listener on_key_statuses_change;
  Listener on_message;

  ExceptionOr<double> GetExpiration() const;
  ExceptionOr<ReturnVal<JsValue>> GetKeyStatuses() const;

  Promise GenerateRequest(MediaKeyInitDataType init_data_type,
                          ByteBuffer init_data);
  Promise Load(const std::string& session_id);
  Promise Update(ByteBuffer response);
  Promise Close();
  Promise Remove();

 private:
  mutable Mutex mutex_;
  std::string session_id_;
  const std::shared_ptr<ImplementationFactory> factory_;
  const std::shared_ptr<Implementation> implementation_;
  const MediaKeySessionType type_;
  EmePromise closed_promise_;
};

class MediaKeySessionFactory
    : public BackingObjectFactory<MediaKeySession, events::EventTarget> {
 public:
  MediaKeySessionFactory();
};

}  // namespace eme
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_EME_MEDIA_KEY_SESSION_H_
