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
#include "src/util/clock.h"

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
using testing::AnyNumber;
using testing::ByMove;
using testing::InSequence;
using testing::Invoke;
using testing::MockFunction;
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

class MockNetworkFilters : public NetworkFilters {
 public:
  MOCK_METHOD2(OnRequestFilter,
               std::future<optional<Error>>(RequestType, Request*));
  MOCK_METHOD2(OnResponseFilter,
               std::future<optional<Error>>(RequestType, Response*));
};

std::future<optional<Error>> MakeFuture(Error error) {
  std::promise<optional<Error>> promise;
  promise.set_value(error);
  return promise.get_future();
}

std::future<optional<Error>> DefaultSchemeCallback(const std::string&,
                                                   RequestType,
                                                   const Request& request,
                                                   SchemePlugin::Client*,
                                                   Response* response) {
  if (request.method == "GET") {
    const std::vector<uint8_t> data = GetMediaFile("dash.mpd");
    response->SetDataCopy(data.data(), data.size());
  }
  response->headers["content-type"] = kMimeType;
  return {};
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

  StrictMock<MockSchemePlugin> scheme;
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

TEST_F(PlayerIntegration, NetworkFilters_BasicFlow) {
  const std::string original_url = "test://foo";
  const std::string url2 = "test://bar";
  const std::vector<uint8_t> temp_data = {1, 2, 3, 4, 5, 6};
  const std::vector<uint8_t> temp_data2 = {7, 8, 9, 0};
  const std::vector<uint8_t> data = GetMediaFile("dash.mpd");

  auto request_filter = [&](RequestType, Request* request) {
    // Verify original properties.
    EXPECT_EQ(request->uris, std::vector<std::string>{original_url});
    EXPECT_EQ(request->method, "GET");
    EXPECT_TRUE(request->headers.empty());
    EXPECT_EQ(request->body_size(), 0);

    // Modify values for future filters.
    request->uris[0] = url2;
    request->method = "WIN";
    request->headers["foo"] = "bar";
    request->SetBodyCopy(temp_data.data(), temp_data.size());
    return std::future<optional<Error>>{};
  };
  auto scheme_plugin = [&](const std::string&, RequestType,
                           const Request& request, SchemePlugin::Client*,
                           Response* response) {
    // Verify request filter set these properties.
    EXPECT_EQ(request.uris, std::vector<std::string>{url2});
    EXPECT_EQ(request.method, "WIN");
    EXPECT_EQ(request.headers.size(), 1);
    EXPECT_EQ(request.headers.at("foo"), "bar");
    EXPECT_EQ(std::vector<uint8_t>(request.body(),
                                   request.body() + request.body_size()),
              temp_data);

    // Fill response for response filter.
    response->uri = url2;
    response->originalUri = original_url;
    response->headers["cat"] = "dog";
    response->timeMs = 666;
    response->SetDataCopy(temp_data2.data(), temp_data2.size());
    return std::future<optional<Error>>{};
  };
  auto response_filter = [&](RequestType, Response* response) {
    // Verify scheme set properties.
    EXPECT_EQ(response->uri, url2);
    EXPECT_EQ(response->originalUri, original_url);
    EXPECT_EQ(response->headers.size(), 1);
    EXPECT_EQ(response->headers.at("cat"), "dog");
    EXPECT_EQ(response->timeMs, 666);
    EXPECT_EQ(std::vector<uint8_t>(response->data(),
                                   response->data() + response->data_size()),
              temp_data2);

    // Set the real response so the load can continue.  If the request succeeds,
    // then these were set properly.
    response->headers["content-type"] = kMimeType;
    response->SetDataCopy(data.data(), data.size());
    return std::future<optional<Error>>{};
  };

  StrictMock<MockSchemePlugin> scheme;
  StrictMock<MockNetworkFilters> filters;
  EXPECT_CALL(scheme, OnNetworkRequest(url2, RequestType::Manifest, _, _, _))
      .WillOnce(Invoke(scheme_plugin));
  EXPECT_CALL(filters, OnRequestFilter(RequestType::Manifest, _))
      .WillOnce(Invoke(request_filter));
  EXPECT_CALL(filters, OnResponseFilter(RequestType::Manifest, _))
      .WillOnce(Invoke(response_filter));
  // Ignore segment requests.
  EXPECT_CALL(filters, OnRequestFilter(RequestType::Segment, _))
      .Times(AnyNumber());
  EXPECT_CALL(filters, OnResponseFilter(RequestType::Segment, _))
      .Times(AnyNumber());

  ASSERT_SUCCESS(g_js_manager->RegisterNetworkScheme("test", &scheme));
  player->AddNetworkFilters(&filters);
  ASSERT_SUCCESS(player->Load(original_url, 0, kMimeType));
  ASSERT_SUCCESS(player->Unload());
}

TEST_F(PlayerIntegration, NetworkFilters_AllowsAsync) {
  const auto kTimeout = std::chrono::milliseconds(200);
  // Use arbitrary numbers here.
  const Error error(3, 5, 7, "Not supported");
  std::promise<optional<Error>> promise1;
  std::promise<optional<Error>> promise2;

  StrictMock<MockNetworkFilters> filters;
  StrictMock<MockSchemePlugin> scheme;
  MockFunction<void()> done;
  {
    InSequence seq;
    EXPECT_CALL(filters, OnRequestFilter(RequestType::Manifest, _))
        .WillOnce(Return(ByMove(promise1.get_future())));
    EXPECT_CALL(done, Call()).Times(1);
    EXPECT_CALL(scheme, OnNetworkRequest(_, RequestType::Manifest, _, _, _))
        .WillOnce(Invoke(&DefaultSchemeCallback));
    EXPECT_CALL(filters, OnResponseFilter(RequestType::Manifest, _))
        .WillOnce(Return(ByMove(promise2.get_future())));
  }

  ASSERT_SUCCESS(g_js_manager->RegisterNetworkScheme("test", &scheme));
  player->AddNetworkFilters(&filters);
  auto load = player->Load("test://foo", 0, kMimeType);

  // Should be waiting for the request filter to finish.
  ASSERT_EQ(load.wait_for(kTimeout), std::future_status::timeout);

  done.Call();
  promise1.set_value({});

  // Should be waiting for the response filter to finish.
  ASSERT_EQ(load.wait_for(kTimeout), std::future_status::timeout);

  promise2.set_value(error);
  EXPECT_TRUE(load.has_error());
}

TEST_F(PlayerIntegration, NetworkFilters_AllowsMultiple) {
  // Use arbitrary numbers here.
  const Error error(3, 5, 7, "Not supported");
  StrictMock<MockNetworkFilters> filters1;
  StrictMock<MockNetworkFilters> filters2;
  StrictMock<MockNetworkFilters> filters3;
  {
    InSequence seq;
    EXPECT_CALL(filters1, OnRequestFilter(RequestType::Manifest, _)).Times(1);
    EXPECT_CALL(filters2, OnRequestFilter(RequestType::Manifest, _)).Times(1);
    EXPECT_CALL(filters3, OnRequestFilter(RequestType::Manifest, _)).Times(1);

    EXPECT_CALL(filters1, OnResponseFilter(RequestType::Manifest, _)).Times(1);
    EXPECT_CALL(filters2, OnResponseFilter(RequestType::Manifest, _)).Times(1);
    EXPECT_CALL(filters3, OnResponseFilter(RequestType::Manifest, _))
        .WillOnce(Return(ByMove(MakeFuture(error))));
  }

  player->AddNetworkFilters(&filters1);
  player->AddNetworkFilters(&filters2);
  player->AddNetworkFilters(&filters3);
  auto load = player->Load(kManifestUrl, 0, kMimeType);
  EXPECT_TRUE(load.has_error());
}

TEST_F(PlayerIntegration, NetworkFilters_RequestReportsErrors) {
  const std::string url = "test://bar";
  // Use arbitrary numbers here.
  const Error error(3, 5, 7, "Not supported");

  StrictMock<MockNetworkFilters> filters;
  EXPECT_CALL(filters, OnRequestFilter(RequestType::Manifest, _))
      .WillOnce(Return(ByMove(MakeFuture(error))));

  player->AddNetworkFilters(&filters);
  auto results = player->Load(url);
  ASSERT_TRUE(results.has_error());
  EXPECT_EQ(results.error().severity, 2);  // CRITICAL
  EXPECT_EQ(results.error().category, 1);  // NETWORK
  EXPECT_EQ(results.error().code, 1006);   // REQUEST_FILTER_ERROR
}

TEST_F(PlayerIntegration, NetworkFilters_ResponseReportsErrors) {
  // Use arbitrary numbers here.
  const Error error(3, 5, 7, "Not supported");

  StrictMock<MockNetworkFilters> filters;
  EXPECT_CALL(filters, OnRequestFilter(RequestType::Manifest, _)).Times(1);
  EXPECT_CALL(filters, OnResponseFilter(RequestType::Manifest, _))
      .WillOnce(Return(ByMove(MakeFuture(error))));

  player->AddNetworkFilters(&filters);
  auto results = player->Load(kManifestUrl);
  ASSERT_TRUE(results.has_error());
  EXPECT_EQ(results.error().severity, 2);  // CRITICAL
  EXPECT_EQ(results.error().category, 1);  // NETWORK
  EXPECT_EQ(results.error().code, 1007);   // RESPONSE_FILTER_ERROR
}

}  // namespace shaka
