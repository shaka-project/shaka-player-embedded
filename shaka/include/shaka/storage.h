// Copyright 2019 Google LLC
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

#ifndef SHAKA_EMBEDDED_STORAGE_H_
#define SHAKA_EMBEDDED_STORAGE_H_

#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "async_results.h"
#include "macros.h"
#include "offline_externs.h"
#include "player.h"

namespace shaka {

class JsManager;

/**
 * Represents a JavaScript shaka.offline.Storage instance.  This handles
 * storing, listing, and deleting stored content.
 *
 * @ingroup player
 */
class SHAKA_EXPORT Storage final {
 public:
  /**
   * Defines an interface for callback functions.  These callbacks are invoked
   * on a background thread.
   */
  class Client {
   public:
    Client();
    Client(const Client&);
    Client(Client&&);
    virtual ~Client();

    Client& operator=(const Client&);
    Client& operator=(Client&&);


    /**
     * Called periodically to report the progress of a download or delete.
     * @param content The stored content that is being downloaded/deleted.
     * @param progress The current progress, 0-1.
     */
    virtual void OnProgress(StoredContent content, double progress);
  };

  /**
   * Creates a new Storage instance.
   * @param engine The JavaScript engine to use.
   * @param player The Player instance to use as a base, can be nullptr.
   */
  Storage(JsManager* engine, Player* player = nullptr);
  Storage(const Storage&) = delete;
  Storage(Storage&&);
  ~Storage();

  Storage& operator=(const Storage&) = delete;
  Storage& operator=(Storage&&);

  /**
   * Gets whether offline storage is supported. Returns true if offline storage
   * is supported for clear content. Support for offline storage of encrypted
   * content will not be determined until storage is attempted.
   */
  static AsyncResults<bool> Support(JsManager* engine);

  /**
   * Delete the on-disk storage and all the content it contains. This should not
   * be done in normal circumstances. Only do it when storage is rendered
   * unusable, such as by a version mismatch. No business logic will be run, and
   * licenses will not be released.
   */
  static AsyncResults<void> DeleteAll(JsManager* engine);


  /**
   * Initializes the Storage object.  This must be called before any other
   * methods.
   */
  AsyncResults<void> Initialize(Client* client = nullptr);

  /**
   * Request that this object be destroyed, releasing all resources and shutting
   * down all operations. Returns a Promise which is resolved when destruction
   * is complete. This Promise should never be rejected.
   */
  AsyncResults<void> Destroy();


  /** Returns true if an asset is currently downloading. */
  AsyncResults<bool> GetStoreInProgress();


  //@{
  /**
   * Sets configuration values for Storage. This is associated with
   * Player.configure and will change the player instance given at
   * initialization.  The path is a '.' separated list of names to reach the
   * configuration.  For example:
   *
   * 'abr.enabled' => {abr: {enabled: value}}
   *
   * @return A future to whether the configuration path was valid.
   */
  AsyncResults<bool> Configure(const std::string& name_path, DefaultValueType);
  AsyncResults<bool> Configure(const std::string& name_path, bool value);
  AsyncResults<bool> Configure(const std::string& name_path, double value);
  AsyncResults<bool> Configure(const std::string& name_path,
                               const std::string& value);
  template <typename T, typename = typename std::enable_if<
                            std::is_arithmetic<T>::value>::type>
  AsyncResults<bool> Configure(const std::string& name_path, T value) {
    // Since there are both bool and double overloads, any other number type
    // will cause an ambiguous call.  This disambiguates the calls.
    return Configure(name_path, static_cast<double>(value));
  }
  AsyncResults<bool> Configure(const std::string& name_path,
                               const char* value) {
    // For some reason, the compiler tries to use the bool overload over the
    // std::string one when passing a char pointer.
    return Configure(name_path, std::string(value));
  }
  //@}


  /**
   * Lists all the stored content available.  This return array of structures
   * representing all stored content. The <code>offlineUri</code> member of the
   * structure is the URI that should be given to <code>Player::Load()</code> to
   * play this piece of content offline.
   */
  AsyncResults<std::vector<StoredContent>> List();

  /**
   * Removes the given stored content. This will also attempt to release the
   * licenses, if any.
   */
  AsyncResults<void> Remove(const std::string& content_uri);

  /**
   * Removes any EME sessions that were not successfully removed before. This
   * returns whether all the sessions were successfully removed.
   */
  AsyncResults<bool> RemoveEmeSessions();

  /**
   * Stores the given manifest. If the content is encrypted, and encrypted
   * content cannot be stored on this platform, the Promise will be rejected
   * with error code 6001, REQUESTED_KEY_SYSTEM_CONFIG_UNAVAILABLE.
   */
  AsyncResults<StoredContent> Store(const std::string& uri);

  /**
   * Stores the given manifest.  This also stores the given data along side the
   * media data so the app can store additional data.
   */
  AsyncResults<StoredContent> Store(
      const std::string& uri,
      const std::unordered_map<std::string, std::string>& app_metadata);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_STORAGE_H_
