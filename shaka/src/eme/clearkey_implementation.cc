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

#include "src/eme/clearkey_implementation.h"

#include <algorithm>
#include <utility>

#include "src/js/base_64.h"
#include "src/js/js_error.h"
#include "src/mapping/js_wrappers.h"
#include "src/util/buffer_reader.h"
#include "src/util/utils.h"

namespace shaka {
namespace eme {

namespace {

bool ParseKeyIds(const Data& data, std::vector<std::string>* base64_keyids) {
  LocalVar<JsValue> data_val =
      ParseJsonString(std::string(data.data(), data.data() + data.size()));
  if (!IsObject(data_val)) {
    LOG(ERROR) << "Init data is not valid JSON.";
    return false;
  }
  LocalVar<JsObject> data_obj = UnsafeJsCast<JsObject>(data_val);
  LocalVar<JsValue> kids = GetMemberRaw(data_obj, "kids");
  if (GetValueType(kids) != proto::ValueType::Array) {
    LOG(ERROR) << "Init data doesn't have valid 'kids' member.";
    return false;
  }

  LocalVar<JsObject> kids_obj = UnsafeJsCast<JsObject>(kids);
  const size_t kid_count = ArrayLength(kids_obj);
  base64_keyids->reserve(kid_count);
  for (size_t i = 0; i < kid_count; i++) {
    LocalVar<JsValue> entry = GetArrayIndexRaw(kids_obj, i);
    if (GetValueType(entry) != proto::ValueType::String) {
      LOG(ERROR) << "Init data doesn't have valid 'kids' member.";
      return false;
    }
    base64_keyids->emplace_back(ConvertToString(entry));
  }
  return true;
}

bool ParsePssh(const Data& data, std::vector<std::string>* base64_keyids) {
  // 4 box size
  // 4 box type
  // 4 version + flags
  // 16 system-id
  // if (version > 0)
  //   4 key id count
  //   for (key id count)
  //     16 key id
  //  4 data size
  //  [data size] data
  constexpr const size_t kSystemIdSize = 16;
  constexpr const size_t kKeyIdSize = 16;
  constexpr const uint8_t kCommonSystemId[] = {
      0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02,
      0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b};

  util::BufferReader reader(data.data(), data.size());
  while (!reader.empty()) {
    const size_t box_start_remaining = reader.BytesRemaining();
    const uint32_t box_size = reader.ReadUint32();

    if (reader.ReadUint32() != 0x70737368) {  // 'pssh'
      LOG(ERROR) << "Init data is not a PSSH box";
      return false;
    }

    const uint8_t version = reader.ReadUint8();
    reader.Skip(3);

    uint8_t system_id[kSystemIdSize];
    if (reader.Read(system_id, kSystemIdSize) != kSystemIdSize) {
      LOG(ERROR) << "Truncated init data";
      return false;
    }

    if (memcmp(system_id, kCommonSystemId, kSystemIdSize) != 0) {
      VLOG(1) << "Ignoring non-common PSSH box";
      const size_t bytes_read = box_start_remaining - reader.BytesRemaining();
      reader.Skip(box_size - bytes_read);
      continue;
    }

    if (version == 0) {
      LOG(ERROR) << "PSSH version 0 is not supported for clear-key";
      return false;
    }

    const uint32_t key_id_count = reader.ReadUint32();
    if (reader.BytesRemaining() / kKeyIdSize < key_id_count) {
      LOG(ERROR) << "Truncated init data";
      return false;
    }

    base64_keyids->reserve(key_id_count);
    for (uint32_t i = 0; i < key_id_count; i++) {
      uint8_t key_id[kKeyIdSize];
      if (reader.Read(key_id, kKeyIdSize) != kKeyIdSize) {
        LOG(ERROR) << "Truncated init data";
        return false;
      }
      base64_keyids->emplace_back(
          js::Base64::EncodeUrl(ByteString(key_id, key_id + kKeyIdSize)));
    }
    return true;
  }

  LOG(ERROR) << "No PSSH box with common system ID found";
  return false;
}

bool ParseAndGenerateRequest(MediaKeyInitDataType type, const Data& data,
                             std::string* message) {
  std::vector<std::string> key_ids;
  switch (type) {
    case MediaKeyInitDataType::KeyIds:
      if (!ParseKeyIds(data, &key_ids))
        return false;
      break;
    case MediaKeyInitDataType::Cenc:
      if (!ParsePssh(data, &key_ids))
        return false;
      break;

    default:
      LOG(ERROR) << "Init data type not supported.";
      return false;
  }

  std::string ids_json;
  for (const std::string& id : key_ids) {
    if (ids_json.empty())
      ids_json = '"' + id + '"';
    else
      ids_json += R"(,")" + id + '"';
  }
  *message = R"({"kids":[)" + ids_json + R"(],"type":"temporary"})";
  return true;
}

// This needs to be a template to access the private type |Session::Key|.
template <typename KeyType>
bool ParseResponse(const Data& data, std::list<KeyType>* keys) {
  LocalVar<JsValue> data_val =
      ParseJsonString(std::string(data.data(), data.data() + data.size()));
  if (!IsObject(data_val)) {
    LOG(ERROR) << "License response is not valid JSON.";
    return false;
  }
  LocalVar<JsObject> data_obj = UnsafeJsCast<JsObject>(data_val);
  LocalVar<JsValue> keys_val = GetMemberRaw(data_obj, "keys");
  if (GetValueType(keys_val) != proto::ValueType::Array) {
    LOG(ERROR) << "License response doesn't contain a valid 'keys' member.";
    return false;
  }

  LocalVar<JsObject> keys_array = UnsafeJsCast<JsObject>(keys_val);
  const size_t key_count = ArrayLength(keys_array);
  for (size_t i = 0; i < key_count; i++) {
    LocalVar<JsValue> entry = GetArrayIndexRaw(keys_array, i);
    if (!IsObject(entry)) {
      LOG(ERROR) << "License response doesn't contain a valid 'keys' member.";
      return false;
    }
    LocalVar<JsObject> entry_obj = UnsafeJsCast<JsObject>(entry);

    LocalVar<JsValue> k_val = GetMemberRaw(entry_obj, "k");
    LocalVar<JsValue> kid_val = GetMemberRaw(entry_obj, "kid");
    if (GetValueType(k_val) != proto::ValueType::String ||
        GetValueType(kid_val) != proto::ValueType::String) {
      LOG(ERROR) << "License response contains invalid key object.";
      return false;
    }

    ExceptionOr<ByteString> k = js::Base64::DecodeUrl(ConvertToString(k_val));
    ExceptionOr<ByteString> kid =
        js::Base64::DecodeUrl(ConvertToString(kid_val));
    if (holds_alternative<js::JsError>(k) ||
        holds_alternative<js::JsError>(kid)) {
      LOG(ERROR) << "License response contains invalid base-64 encoding.";
      return false;
    }
    if (get<ByteString>(k).size() != 16 || get<ByteString>(kid).size() != 16) {
      LOG(ERROR) << "Key or key ID is not correct size.";
      return false;
    }
    keys->emplace_back(std::move(get<ByteString>(kid)),
                       std::move(get<ByteString>(k)));
  }

  return true;
}

}  // namespace

ClearKeyImplementation::ClearKeyImplementation(ImplementationHelper* helper)
    : helper_(helper), cur_session_id_(0) {}
ClearKeyImplementation::~ClearKeyImplementation() {}

bool ClearKeyImplementation::GetExpiration(const std::string& session_id,
                                           int64_t* expiration) const {
  *expiration = -1;
  return sessions_.count(session_id) != 0;
}

bool ClearKeyImplementation::GetKeyStatuses(
    const std::string& session_id, std::vector<KeyStatusInfo>* statuses) const {
  std::unique_lock<std::mutex> lock(mutex_);
  if (sessions_.count(session_id) == 0)
    return false;

  statuses->clear();
  for (auto& key : sessions_.at(session_id).keys)
    statuses->emplace_back(key.key_id, MediaKeyStatus::Usable);
  return true;
}

void ClearKeyImplementation::SetServerCertificate(EmePromise promise,
                                                  Data /* cert */) {
  promise.ResolveWith(false);
}

void ClearKeyImplementation::CreateSessionAndGenerateRequest(
    EmePromise promise, std::function<void(const std::string&)> set_session_id,
    MediaKeySessionType session_type, MediaKeyInitDataType init_data_type,
    Data data) {
  DCHECK(session_type == MediaKeySessionType::Temporary);

  std::unique_lock<std::mutex> lock(mutex_);

  const std::string session_id = std::to_string(++cur_session_id_);
  // The indexer will create a new object since it doesn't exist.
  Session* session = &sessions_[session_id];
  DCHECK(!session->callable);

  std::string message;
  if (!ParseAndGenerateRequest(init_data_type, data, &message)) {
    promise.Reject(ExceptionType::TypeError,
                   "Invalid initialization data given.");
    return;
  }

  session->callable = true;
  set_session_id(session_id);
  helper_->OnMessage(session_id, MediaKeyMessageType::LicenseRequest,
                     reinterpret_cast<const uint8_t*>(message.c_str()),
                     message.size());
  promise.Resolve();
}

void ClearKeyImplementation::Load(const std::string& /* session_id */,
                                  EmePromise promise) {
  promise.Reject(ExceptionType::NotSupported,
                 "Clear-key doesn't support persistent licences.");
}

void ClearKeyImplementation::Update(const std::string& session_id,
                                    EmePromise promise, Data data) {
  std::unique_lock<std::mutex> lock(mutex_);

  if (sessions_.count(session_id) == 0) {
    promise.Reject(ExceptionType::InvalidState,
                   "Unable to find given session ID.");
    return;
  }
  Session* session = &sessions_.at(session_id);

  if (!session->callable) {
    promise.Reject(ExceptionType::InvalidState, "Not expecting an update.");
    return;
  }

  std::list<Session::Key> keys;
  if (!ParseResponse(data, &keys)) {
    promise.Reject(ExceptionType::InvalidState, "Invalid response data.");
    return;
  }

  session->callable = false;
  // Move all keys into the session.
  session->keys.splice(session->keys.end(), std::move(keys));
  helper_->OnKeyStatusChange(session_id);
  promise.Resolve();
}

void ClearKeyImplementation::Close(const std::string& session_id,
                                   EmePromise promise) {
  std::unique_lock<std::mutex> lock(mutex_);
  sessions_.erase(session_id);
  promise.Resolve();
}

void ClearKeyImplementation::Remove(const std::string& /* session_id */,
                                    EmePromise promise) {
  promise.Reject(ExceptionType::NotSupported,
                 "Clear-key doesn't support persistent licences.");
}

DecryptStatus ClearKeyImplementation::Decrypt(const FrameEncryptionInfo* info,
                                              const uint8_t* data,
                                              size_t data_size,
                                              uint8_t* dest) const {
  std::unique_lock<std::mutex> lock(mutex_);

  const std::vector<uint8_t>* key = nullptr;
  for (auto& session_pair : sessions_) {
    for (auto& cur_key : session_pair.second.keys) {
      if (cur_key.key_id == info->key_id) {
        key = &cur_key.key;
        break;
      }
    }
  }
  if (!key) {
    LOG(ERROR) << "Unable to find key ID: "
               << util::ToHexString(info->key_id.data(), info->key_id.size());
    return DecryptStatus::KeyNotFound;
  }

  util::Decryptor decryptor(info->scheme, *key, info->iv);
  if (info->subsamples.empty()) {
    return DecryptBlock(info, data, data_size, 0, dest, &decryptor);
  } else {
    size_t block_offset = 0;
    for (const auto& subsample : info->subsamples) {
      if (data_size < subsample.clear_bytes ||
          data_size - subsample.clear_bytes < subsample.protected_bytes) {
        LOG(ERROR) << "Input data not large enough for subsamples";
        return DecryptStatus::OtherError;
      }

      // The clear portion appears first.
      memcpy(dest, data, subsample.clear_bytes);
      data += subsample.clear_bytes;
      dest += subsample.clear_bytes;
      data_size -= subsample.clear_bytes;

      // Then the encrypted portion.
      const auto ret = DecryptBlock(info, data, subsample.protected_bytes,
                                    block_offset, dest, &decryptor);
      if (ret != DecryptStatus::Success)
        return ret;
      data += subsample.protected_bytes;
      dest += subsample.protected_bytes;
      data_size -= subsample.protected_bytes;
      block_offset =
          (block_offset + subsample.protected_bytes) % AES_BLOCK_SIZE;

      // iv changing is handled by Decryptor.
    }
    if (data_size != 0) {
      LOG(ERROR) << "Data remaining after subsample handling";
      return DecryptStatus::OtherError;
    }
    return DecryptStatus::Success;
  }
}

DecryptStatus ClearKeyImplementation::DecryptBlock(
    const FrameEncryptionInfo* info, const uint8_t* data, size_t data_size,
    size_t block_offset, uint8_t* dest, util::Decryptor* decryptor) const {
  size_t num_bytes_read = 0;
  if (block_offset != 0) {
    if (info->pattern.clear_blocks != 0) {
      LOG(ERROR) << "Cannot have block offset when using pattern encryption";
      return DecryptStatus::OtherError;
    }

    num_bytes_read = std::min<size_t>(data_size, AES_BLOCK_SIZE - block_offset);
    if (!decryptor->DecryptPartialBlock(data, num_bytes_read, block_offset,
                                        dest)) {
      return DecryptStatus::OtherError;
    }
  }

  if (info->pattern.clear_blocks != 0) {
    const size_t protected_size =
        AES_BLOCK_SIZE * info->pattern.encrypted_blocks;
    const size_t clear_size = AES_BLOCK_SIZE * info->pattern.clear_blocks;
    const size_t pattern_size_in_blocks =
        info->pattern.encrypted_blocks + info->pattern.clear_blocks;
    const size_t data_size_in_blocks = data_size / AES_BLOCK_SIZE;
    for (size_t i = 0; i < data_size_in_blocks / pattern_size_in_blocks; i++) {
      if (!decryptor->Decrypt(data + num_bytes_read, protected_size,
                              dest + num_bytes_read)) {
        return DecryptStatus::OtherError;
      }
      num_bytes_read += protected_size;

      memcpy(dest + num_bytes_read, data + num_bytes_read, clear_size);
      num_bytes_read += clear_size;
    }

    // If the last pattern block isn't big enough for the whole
    // encrypted_blocks, then it is ignored.
    if (data_size_in_blocks % pattern_size_in_blocks >=
        info->pattern.encrypted_blocks) {
      if (!decryptor->Decrypt(data + num_bytes_read, protected_size,
                              dest + num_bytes_read)) {
        return DecryptStatus::OtherError;
      }
      num_bytes_read += protected_size;
    }

    memcpy(dest + num_bytes_read, data + num_bytes_read,
           data_size - num_bytes_read);
  } else {
    if (!decryptor->Decrypt(data + num_bytes_read, data_size - num_bytes_read,
                            dest + num_bytes_read)) {
      return DecryptStatus::OtherError;
    }
  }

  return DecryptStatus::Success;
}

void ClearKeyImplementation::LoadKeyForTesting(std::vector<uint8_t> key_id,
                                               std::vector<uint8_t> key) {
  std::unique_lock<std::mutex> lock(mutex_);
  const std::string session_id = std::to_string(++cur_session_id_);
  sessions_.emplace(session_id, Session());
  sessions_.at(session_id).keys.emplace_back(std::move(key_id), std::move(key));
}

ClearKeyImplementation::Session::Key::Key(std::vector<uint8_t> key_id,
                                          std::vector<uint8_t> key)
    : key_id(std::move(key_id)), key(std::move(key)) {}
ClearKeyImplementation::Session::Key::~Key() {}

ClearKeyImplementation::Session::Session() {}
ClearKeyImplementation::Session::~Session() {}

ClearKeyImplementation::Session::Session(Session&&) = default;
ClearKeyImplementation::Session& ClearKeyImplementation::Session::operator=(
    Session&&) = default;

}  // namespace eme
}  // namespace shaka
