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

#ifndef SHAKA_EMBEDDED_EME_IMPLEMENTATION_HELPER_H_
#define SHAKA_EMBEDDED_EME_IMPLEMENTATION_HELPER_H_

#include <string>

#include "../macros.h"
#include "configuration.h"

namespace shaka {
namespace eme {

/**
 * An interface to the JavaScript backing of EME.  This includes callbacks that
 * are invoked by an EME implementation.
 *
 * This type is thread-safe.
 *
 * This MUST NOT be subclassed by the app.  Only the public members are part
 * of the public ABI.  This allows us to add new members without breaking ABI
 * compatibility.
 *
 * @ingroup eme
 */
class SHAKA_EXPORT ImplementationHelper {
 public:
  // Note that we may not be able to add new members without breaking ABI on
  // Windows.  We may be able to do it so long as we use clang.

  /**
   * Gets the directory on the filesystem that should be used for data storage.
   * This directory is specific for this EME implementation, so any files in it
   * are for this implementation.  ALL data MUST be in this directory.
   */
  virtual std::string DataPathPrefix() const = 0;


  /**
   * An event callback that should be called when a message should be sent to
   * the JavaScript application.  This only schedules a JavaScript event, it
   * does not dispatch it.
   *
   * @param session_id The ID of the session that will receive the message.
   * @param message_type The type of the message.
   * @param data The data contents of the message.
   * @param data_size The number of bytes in |data|.
   */
  virtual void OnMessage(const std::string& session_id,
                         MediaKeyMessageType message_type, const uint8_t* data,
                         size_t data_size) const = 0;

  /**
   * An event callback that should be called when the key status changes.  This
   * schedules a JavaScript event, but doesn't dispatch it.
   * @param session_id The ID of the session whose key statuses changed.
   */
  virtual void OnKeyStatusChange(const std::string& session_id) const = 0;

 protected:
  virtual ~ImplementationHelper();
};

}  // namespace eme
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_EME_IMPLEMENTATION_HELPER_H_
