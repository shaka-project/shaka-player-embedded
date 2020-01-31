// Copyright 2020 Google LLC
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "shaka/js_manager.h"
#include "shaka/net.h"
#include "shaka/player.h"
#include "src/test/global_fields.h"
#include "src/test/media_files.h"

namespace shaka {

namespace {

#define ASSERT_SUCCESS(code)             \
  do {                                   \
    auto results = (code);               \
    if (results.has_error()) {           \
      FAIL() << results.error().message; \
      return;                            \
    }                                    \
  } while (false)

#define EXPECT_SUCCESS(code)             \
  do {                                   \
    auto results = (code);               \
    if (results.has_error()) {           \
      FAIL() << results.error().message; \
    }                                    \
  } while (false)

#define ASSERT_SUCCESS_WITH_RESULTS(code, res) \
  do {                                         \
    auto results = (code);                     \
    if (results.has_error()) {                 \
      FAIL() << results.error().message;       \
      return;                                  \
    }                                          \
    res = results.results();                   \
  } while (false)

using testing::_;
using testing::ByMove;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

constexpr const char* kManifestUrl =
    "https://storage.googleapis.com/shaka-demo-assets/angel-one/dash.mpd";
constexpr const char* kMimeType = "application/dash+xml";

class MockClient : public Player::Client {
 public:
  MOCK_METHOD1(OnError, void(const Error& error));
  MOCK_METHOD1(OnBuffering, void(bool is_buffering));
};

class MockSchemePlugin : public SchemePlugin {
 public:
  MOCK_METHOD5(OnNetworkRequest,
               std::future<optional<Error>>(const std::string&, RequestType,
                                            const Request&, Client*,
                                            Response*));
};

std::future<optional<Error>> MakeFuture(Error error) {
  std::promise<optional<Error>> promise;
  promise.set_value(error);
  return promise.get_future();
}

}  // namespace

class PlayerIntegration : public testing::Test {
 public:
  void SetUp() override {
    EXPECT_CALL(client, OnError(_)).Times(0);

    player.reset(new Player(g_js_manager));
    ASSERT_SUCCESS(player->Initialize(&client, g_media_player));
  }

  void TearDown() override {
    EXPECT_SUCCESS(g_js_manager->UnregisterNetworkScheme("test"));
    EXPECT_SUCCESS(player->Destroy());
    player.reset();
  }

  NiceMock<MockClient> client;
  std::unique_ptr<Player> player;
};

TEST_F(PlayerIntegration, Player_BasicFlow) {
  bool field;
  ASSERT_SUCCESS(player->Load(kManifestUrl));
  ASSERT_SUCCESS_WITH_RESULTS(player->IsAudioOnly(), field);
  EXPECT_FALSE(field);
  ASSERT_SUCCESS_WITH_RESULTS(player->IsLive(), field);
  EXPECT_FALSE(field);
  ASSERT_SUCCESS(player->Unload());
}

TEST_F(PlayerIntegration, SchemePlugin_BasicFlow) {
  const std::string url = "test://foo";
  const std::vector<uint8_t> data = GetMediaFile("dash.mpd");
  auto cb = [&](const std::string&, RequestType, const Request& request,
                SchemePlugin::Client* client, Response* response) {
    EXPECT_EQ(request.uris, std::vector<std::string>{url});
    EXPECT_EQ(request.method, "GET");
    EXPECT_EQ(request.body_size(), 0);

    response->originalUri = response->uri = url;
    response->headers["content-type"] = kMimeType;
    response->SetDataCopy(data.data(), data.size());

    client->OnProgress(0, data.size(), data.size());

    return std::future<optional<Error>>{};
  };

  StrictMock<MockSchemePlugin> scheme;
  EXPECT_CALL(scheme, OnNetworkRequest(url, RequestType::Manifest, _, _, _))
      .WillOnce(Invoke(cb));

  ASSERT_SUCCESS(g_js_manager->RegisterNetworkScheme("test", &scheme));
  ASSERT_SUCCESS(player->Load(url, 0, kMimeType));
  ASSERT_SUCCESS(player->Unload());
}

TEST_F(PlayerIntegration, SchemePlugin_ReportsErrors) {
  std::string url = "test://foo";
  // Use arbitrary numbers here.
  Error error(3, 5, 7, "Not supported");

  MockSchemePlugin scheme;
  EXPECT_CALL(scheme, OnNetworkRequest(url, RequestType::Manifest, _, _, _))
      .WillOnce(Return(ByMove(MakeFuture(error))));

  ASSERT_SUCCESS(g_js_manager->RegisterNetworkScheme("test", &scheme));
  // Request should fail.
  auto results = player->Load(url);
  ASSERT_TRUE(results.has_error());
  EXPECT_EQ(results.error().severity, 2);  // Should override to CRITICAL
  EXPECT_EQ(results.error().category, error.category);
  EXPECT_EQ(results.error().code, error.code);
}

}  // namespace shaka
