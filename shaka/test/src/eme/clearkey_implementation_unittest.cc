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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/mapping/byte_buffer.h"
#include "src/public/eme_promise_impl.h"

namespace shaka {
namespace eme {

namespace {

class MockImplementationHelper : public ImplementationHelper {
 public:
  MOCK_CONST_METHOD0(DataPathPrefix, std::string());
  MOCK_CONST_METHOD4(OnMessage, void(const std::string&, MediaKeyMessageType,
                                     const uint8_t*, size_t));
  MOCK_CONST_METHOD1(OnKeyStatusChange, void(const std::string&));
};

}  // namespace

constexpr const uint8_t kKeyId[] = {'1', '2', '3', '4', '5', '6', '7', '8',
                                    '9', '0', '1', '2', '3', '4', '5', '6'};
constexpr const uint8_t kKey[] = {'1', '2', '3', '4', '5', '6', '7', '8',
                                  '9', '0', '1', '2', '3', '4', '5', '6'};
constexpr const uint8_t kClearData[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                                        0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
                                        0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6};
constexpr const uint8_t kEncryptedData[] = {
    0xaa, 0x33, 0x82, 0x87, 0x2b, 0x56, 0x0b, 0xda, 0xa5, 0xb0, 0xad, 0xe3,
    0xe1, 0x4a, 0x29, 0x56, 0x66, 0x16, 0x65, 0xbd, 0xe0, 0xfe, 0x95};
constexpr const int kBlockOffset = 7;
// kClearData encrypted with a block offset of 7.
constexpr const uint8_t kBlockOffsetEncryptedData[] = {
    0xdd, 0xac, 0xbb, 0xa4, 0xec, 0xe8, 0x41, 0x20, 0x51, 0x6f, 0x1d, 0x6c,
    0xb2, 0xe9, 0xf5, 0x9c, 0xfe, 0xc6, 0xe6, 0xe6, 0x6b, 0x76, 0xcd};
constexpr const uint8_t kIv[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                                 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};

using ::testing::_;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::StrictMock;

// There are more tests in JavaScript: //shaka/test/tests/eme.js.

class ClearKeyImplementationTest : public testing::Test {
 protected:
  class MockEmePromiseImpl : public EmePromise::Impl {
   public:
    MOCK_METHOD0(Resolve, void());
    MOCK_METHOD1(ResolveWith, void(bool));
    MOCK_METHOD2(Reject, void(ExceptionType, const std::string&));
  };

  void LoadKeyForTesting(ClearKeyImplementation* clear_key,
                         std::vector<uint8_t> key_id,
                         std::vector<uint8_t> key) {
    clear_key->LoadKeyForTesting(std::move(key_id), std::move(key));
  }

  Data CreateData(const uint8_t* data, size_t size) {
    ByteBuffer buffer(data, size);
    return Data(&buffer);
  }

  EmePromise CreateEmePromise(MockEmePromiseImpl* mock) {
    auto noop = [](EmePromise::Impl*) {};
    std::shared_ptr<EmePromise::Impl> impl(mock, noop);
    return EmePromise(impl);
  }
};

TEST_F(ClearKeyImplementationTest, Decrypt) {
  NiceMock<MockImplementationHelper> helper;
  ClearKeyImplementation clear_key(&helper);

  LoadKeyForTesting(&clear_key,
                    std::vector<uint8_t>(kKeyId, kKeyId + sizeof(kKeyId)),
                    std::vector<uint8_t>(kKey, kKey + sizeof(kKey)));

  // Decryption on a block boundary.
  {
    std::vector<uint8_t> data(kEncryptedData, kEncryptedData + AES_BLOCK_SIZE);
    EXPECT_EQ(clear_key.Decrypt(EncryptionScheme::AesCtr, EncryptionPattern(),
                                0, kKeyId, sizeof(kKeyId), kIv, sizeof(kIv),
                                data.data(), data.size(), data.data()),
              DecryptStatus::Success);
    EXPECT_EQ(data,
              std::vector<uint8_t>(kClearData, kClearData + AES_BLOCK_SIZE));
  }

  // Decryption with a partial block at the end.
  {
    std::vector<uint8_t> data(kEncryptedData,
                              kEncryptedData + sizeof(kEncryptedData));
    EXPECT_EQ(clear_key.Decrypt(EncryptionScheme::AesCtr, EncryptionPattern(),
                                0, kKeyId, sizeof(kKeyId), kIv, sizeof(kIv),
                                data.data(), data.size(), data.data()),
              DecryptStatus::Success);
    EXPECT_EQ(data, std::vector<uint8_t>(kClearData,
                                         kClearData + sizeof(kClearData)));
  }

  // Decryption with a block offset and a second block.
  {
    std::vector<uint8_t> data(
        kBlockOffsetEncryptedData,
        kBlockOffsetEncryptedData + sizeof(kBlockOffsetEncryptedData));
    EXPECT_EQ(
        clear_key.Decrypt(EncryptionScheme::AesCtr, EncryptionPattern(),
                          kBlockOffset, kKeyId, sizeof(kKeyId), kIv,
                          sizeof(kIv), data.data(), data.size(), data.data()),
        DecryptStatus::Success);
    EXPECT_EQ(data, std::vector<uint8_t>(kClearData,
                                         kClearData + sizeof(kClearData)));
  }

  // Decryption with a block offset that doesn't fill a block.
  {
    constexpr const size_t kSize = 5;
    std::vector<uint8_t> data(kBlockOffsetEncryptedData,
                              kBlockOffsetEncryptedData + kSize);
    EXPECT_EQ(
        clear_key.Decrypt(EncryptionScheme::AesCtr, EncryptionPattern(),
                          kBlockOffset, kKeyId, sizeof(kKeyId), kIv,
                          sizeof(kIv), data.data(), data.size(), data.data()),
        DecryptStatus::Success);
    EXPECT_EQ(data, std::vector<uint8_t>(kClearData, kClearData + kSize));
  }
}

TEST_F(ClearKeyImplementationTest, Decrypt_KeyNotFound) {
  NiceMock<MockImplementationHelper> helper;
  ClearKeyImplementation clear_key(&helper);

  std::vector<uint8_t> real_key_id(16, 1);
  ASSERT_EQ(real_key_id.size(), 16u);
  LoadKeyForTesting(&clear_key, real_key_id,
                    std::vector<uint8_t>(kKey, kKey + sizeof(kKey)));

  std::vector<uint8_t> data(kEncryptedData, kEncryptedData + AES_BLOCK_SIZE);
  std::vector<uint8_t> key_id(16, 0);
  ASSERT_EQ(key_id.size(), 16u);
  EXPECT_EQ(clear_key.Decrypt(EncryptionScheme::AesCtr, EncryptionPattern(), 0,
                              key_id.data(), key_id.size(), kIv, sizeof(kIv),
                              data.data(), data.size(), data.data()),
            DecryptStatus::KeyNotFound);
}

TEST_F(ClearKeyImplementationTest, HandlesMissingSessionId) {
  StrictMock<MockImplementationHelper> helper;
  StrictMock<MockEmePromiseImpl> promise_impl;
  MockFunction<void(int)> call;

  {
    InSequence seq;
    EXPECT_CALL(call, Call(0)).Times(1);
    EXPECT_CALL(promise_impl, Reject(_, _));
    EXPECT_CALL(call, Call(1)).Times(1);
    EXPECT_CALL(promise_impl, Reject(_, _));
  }

  ClearKeyImplementation clear_key(&helper);

  int64_t expiration;
  EXPECT_FALSE(clear_key.GetExpiration("nope", &expiration));
  std::vector<KeyStatusInfo> statuses;
  EXPECT_FALSE(clear_key.GetKeyStatuses("nope", &statuses));
  call.Call(0);

  clear_key.Load("nope", CreateEmePromise(&promise_impl));
  call.Call(1);

  constexpr const uint8_t kResponse[] =
      "{\"keys\":[{}],\"type\":\"temporary\"}";
  clear_key.Update("nope", CreateEmePromise(&promise_impl),
                   CreateData(kResponse, sizeof(kResponse) - 1));

  // Note that close() on an unknown session is ignored.
}

}  // namespace eme
}  // namespace shaka
