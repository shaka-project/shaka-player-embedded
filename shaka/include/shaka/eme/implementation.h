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

#ifndef SHAKA_EMBEDDED_EME_IMPLEMENTATION_H_
#define SHAKA_EMBEDDED_EME_IMPLEMENTATION_H_

#include <functional>
#include <string>
#include <vector>

#include "../macros.h"
#include "configuration.h"
#include "data.h"
#include "eme_promise.h"

namespace shaka {
namespace eme {

/**
 * Defines a pair of a key ID and its key status.
 * @ingroup eme
 */
struct SHAKA_EXPORT KeyStatusInfo final {
  KeyStatusInfo();
  KeyStatusInfo(const std::vector<uint8_t>& key_id, MediaKeyStatus status);

  // Needed to satisfy Chromium-style.
  KeyStatusInfo(KeyStatusInfo&&);
  KeyStatusInfo(const KeyStatusInfo&);
  KeyStatusInfo& operator=(KeyStatusInfo&&);
  KeyStatusInfo& operator=(const KeyStatusInfo&);
  ~KeyStatusInfo();

  std::vector<uint8_t> key_id;
  MediaKeyStatus status;
};

inline KeyStatusInfo::KeyStatusInfo() : status(MediaKeyStatus::Usable) {}
inline KeyStatusInfo::KeyStatusInfo(const std::vector<uint8_t>& key_id,
                                    MediaKeyStatus status)
    : key_id(std::move(key_id)), status(status) {}

inline KeyStatusInfo::KeyStatusInfo(KeyStatusInfo&&) = default;
inline KeyStatusInfo::KeyStatusInfo(const KeyStatusInfo&) = default;
inline KeyStatusInfo& KeyStatusInfo::operator=(KeyStatusInfo&&) = default;
inline KeyStatusInfo& KeyStatusInfo::operator=(const KeyStatusInfo&) = default;
inline KeyStatusInfo::~KeyStatusInfo() {}


/**
 * An interface for an EME implementation instance.  This represents an adapter
 * to a CDM instance.  This is a one-to-one mapping to a MediaKeys object in
 * EME.  This should create and manage a single CDM instance.  This object must
 * remain alive until the Destroy() method is called.
 *
 * This can spawn background threads as needed to monitor the system.  These
 * thread(s) must be deleted inside Destroy().
 *
 * It is ok to manipulate the filesystem, but it should be done inside the
 * ImplementationHelper::DataPathPrefix directory.
 *
 * Many of the actions here are asynchronous.  Some are completed by the end
 * of the call here, but are run asynchronously with respect to JavaScript.  In
 * either case, those methods are given a |promise|.  Once the operation is
 * complete (error or success), one of the methods on it MUST be called.
 * It is ok to synchronously call those methods.
 *
 * Most methods here are only called on the JS main thread; the exception is
 * Decrypt, which can be called from any thread, including concurrently with
 * other Decrypt calls.  It is highly suggested to avoid exclusive locks in
 * Decrypt so we can get parallel decrypt operations.
 *
 * @ingroup eme
 */
class SHAKA_EXPORT Implementation {
 public:
  // Since this is intended to be subclassed by the app, this type cannot be
  // changed without breaking ABI compatibility.  This includes adding
  // new virtual methods.

  virtual ~Implementation();

  /**
   * Destroys the object and frees up any memory (including |*this|).  This will
   * be called when the respective EME instances are garbage collected.
   */
  virtual void Destroy() = 0;


  /**
   * Gets the expiration of the session.  This should give the time in
   * milliseconds since the epoch, or -1 if the session never expires.
   *
   * @param session_id The ID of the session to get the expiration for.
   * @param expiration [OUT] Where to put the expiration time.
   * @returns True on success, false on error.
   */
  virtual bool GetExpiration(const std::string& session_id,
                             int64_t* expiration) const = 0;

  /**
   * Gets the status of each key in the given session.  These values can be
   * cached to avoid extra overhead.  This means that the key status may have
   * changed but not be reflected yet (e.g. they may have expired).
   *
   * @param session_id The ID of the session to query.
   * @param statuses [OUT] Where to put the resulting statuses.
   * @returns True on success, false on error.
   */
  virtual bool GetKeyStatuses(const std::string& session_id,
                              std::vector<KeyStatusInfo>* statuses) const = 0;


  /**
   * Sets the server certificate for the CDM.
   *
   * This should use EmePromise::ResolveWith() and pass true for supported and
   * false for not supported.  This should only reject for errors in the
   * certificate.
   *
   * @param promise The Promise object.
   * @param cert The data contents of the certificate.
   */
  virtual void SetServerCertificate(EmePromise promise, Data cert) = 0;

  /**
   * This method creates a new session and generates a license request.  This
   * is only called for new session, not for loading persistent sessions.
   *
   * This should call @a set_session_id before sending any messages so the
   * session ID is set.  The function must only be called once.
   *
   * This method should create a message to send the license request.  This will
   * only be called with init data types where
   * ImplementationFactory::SupportsInitDataType() returns true.
   *
   * The Promise should be resolved when the request has been generated, NOT
   * when the response comes back.  This should call
   * ImplementationHelper::OnMessage() before resolving the Promise.
   *
   * There are situations where this may not generate the license request
   * immediately, for example if the device isn't provisioned.  This will still
   * generate a message, but it may not be a license request.
   *
   * @param promise The Promise object.
   * @param set_session_id A function that accepts the session ID of the new
   *   session.
   * @param session_type The type of session to create.
   * @param init_data_type The type of init data given
   * @param data The data contents of the request.
   */
  virtual void CreateSessionAndGenerateRequest(
      EmePromise promise,
      std::function<void(const std::string&)> set_session_id,
      MediaKeySessionType session_type, MediaKeyInitDataType init_data_type,
      Data data) = 0;

  /**
   * Loads the given session from persistent storage.
   *
   * This should use ResolveWith() and pass true if the session was found, false
   * if the session didn't exist.  This should still reject for errors.
   *
   * @param session_id The session ID to load.
   * @param promise The Promise object.
   */
  virtual void Load(const std::string& session_id, EmePromise promise) = 0;

  /**
   * Updates the given session with a response from the server.
   *
   * @param session_id The session ID to use.
   * @param promise The Promise object.
   * @param data The data contents of the response.
   */
  virtual void Update(const std::string& session_id, EmePromise promise,
                      Data data) = 0;

  /**
   * Closes the given session.  This does NOT delete persistent sessions, it
   * only closes the current running session and any runtime data.
   *
   * @param session_id The session ID to close.
   * @param promise The Promise object.
   */
  virtual void Close(const std::string& session_id, EmePromise promise) = 0;

  /**
   * Removes any persistent data associated with the given session.
   *
   * This should generate a 'license-release' message.  The session should not
   * actually be deleted until the response is given to Update().  However, the
   * Promise should be resolved once the message is generated.
   *
   * @param session_id The session ID to delete.
   * @param promise The Promise object.
   */
  virtual void Remove(const std::string& session_id, EmePromise promise) = 0;


  /**
   * Decrypts the given data.  This is given a whole frame and is expected to
   * decrypt the encrypted portions and copy over clear portions.  This method
   * doesn't need to handle containers or codecs, all it needs to do is decrypt
   * and copy the data.  If the data needs to be processed before decryption
   * (e.g. for MPEG2-TS), it is done by the caller.
   *
   * If pattern is (0, 0), then this is not using pattern encryption (e.g. for
   * "cenc" or "cbc1").
   *
   * @param info Contains information about how the frame is encrypted.
   * @param data The data to decrypt.
   * @param data_size The size of |data|.
   * @param dest The destination buffer to hold the decrypted data.  Is at least
   *   |data_size| bytes large.
   * @returns The resulting status code.
   */
  virtual DecryptStatus Decrypt(const FrameEncryptionInfo* info,
                                const uint8_t* data, size_t data_size,
                                uint8_t* dest) const = 0;
};

}  // namespace eme
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_EME_IMPLEMENTATION_H_
