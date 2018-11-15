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

#ifndef SHAKA_EMBEDDED_EME_CLEARKEY_FACTORY_H_
#define SHAKA_EMBEDDED_EME_CLEARKEY_FACTORY_H_

#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "shaka/eme/implementation.h"
#include "shaka/eme/implementation_helper.h"

#define AES_BLOCK_SIZE 16u

namespace shaka {

namespace media {
class MediaProcessorIntegration;
class MediaProcessorDecryptIntegration;
}  // namespace media

namespace eme {

class ClearKeyImplementation final : public Implementation {
 public:
  explicit ClearKeyImplementation(ImplementationHelper* helper);
  ~ClearKeyImplementation() override;

  void Destroy() override;

  bool GetExpiration(const std::string& session_id,
                     int64_t* expiration) const override;

  bool GetKeyStatuses(const std::string& session_id,
                      std::vector<KeyStatusInfo>* statuses) const override;

  void SetServerCertificate(EmePromise promise, Data cert) override;

  void CreateSessionAndGenerateRequest(
      EmePromise promise,
      std::function<void(const std::string&)> set_session_id,
      MediaKeySessionType session_type, MediaKeyInitDataType init_data_type,
      Data data) override;

  void Load(const std::string& session_id, EmePromise promise) override;

  void Update(const std::string& session_id, EmePromise promise,
              Data data) override;

  void Close(const std::string& session_id, EmePromise promise) override;

  void Remove(const std::string& session_id, EmePromise promise) override;

  DecryptStatus Decrypt(EncryptionScheme scheme, EncryptionPattern pattern,
                        uint32_t block_offset, const uint8_t* key_id,
                        size_t key_id_size, const uint8_t* iv, size_t iv_size,
                        const uint8_t* data, size_t data_size,
                        uint8_t* dest) const override;

 private:
  struct Session {
    struct Key {
      Key(std::vector<uint8_t> key_id, std::vector<uint8_t> key);
      ~Key();

      // TODO: Consider storing keys in OpenSSL key objects instead.
      std::vector<uint8_t> key_id;
      std::vector<uint8_t> key;  // This contains the raw AES key.
    };

    Session();
    ~Session();

    Session(Session&&);
    Session(const Session&) = delete;
    Session& operator=(Session&&);
    Session& operator=(const Session&) = delete;

    std::list<Key> keys;
    bool callable = false;
  };

  friend class ClearKeyImplementationTest;
  friend class media::MediaProcessorIntegration;
  friend class media::MediaProcessorDecryptIntegration;

  void LoadKeyForTesting(std::vector<uint8_t> key_id, std::vector<uint8_t> key);

  mutable std::mutex mutex_;
  std::unordered_map<std::string, Session> sessions_;
  ImplementationHelper* helper_;
  uint32_t cur_session_id_;
};

}  // namespace eme
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_EME_CLEARKEY_FACTORY_H_
