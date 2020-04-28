// Copyright 2018 Google LLC
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

#import "shaka/ShakaPlayer.h"

#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "shaka/ShakaPlayerEmbedded.h"

#include "src/core/js_manager_impl.h"
#include "src/debug/mutex.h"
#include "src/js/manifest+Internal.h"
#include "src/js/player_externs+Internal.h"
#include "src/js/stats+Internal.h"
#include "src/js/track+Internal.h"
#include "src/media/ios/ios_video_renderer.h"
#include "src/public/ShakaPlayer+Internal.h"
#include "src/public/error_objc+Internal.h"
#include "src/public/net_objc+Internal.h"
#include "src/util/macros.h"
#include "src/util/objc_utils.h"
#include "src/util/utils.h"


namespace {

BEGIN_ALLOW_COMPLEX_STATICS
static std::weak_ptr<shaka::JsManager> gJsEngine;
END_ALLOW_COMPLEX_STATICS

class NativeClient final : public shaka::Player::Client, public shaka::media::MediaPlayer::Client {
 public:
  NativeClient() {}

  void OnError(const shaka::Error &error) override {
    ShakaPlayerError *objc_error = [[ShakaPlayerError alloc] initWithError:error];
    shaka::util::DispatchObjcEvent(_client, @selector(onPlayer:error:), _shakaPlayer, objc_error);
  }

  void OnBuffering(bool is_buffering) override {
    shaka::util::DispatchObjcEvent(_client, @selector(onPlayer:bufferingChange:), _shakaPlayer,
                                   is_buffering);
  }


  void OnPlaybackStateChanged(shaka::media::VideoPlaybackState old_state,
                              shaka::media::VideoPlaybackState new_state) override {
    switch (new_state) {
      case shaka::media::VideoPlaybackState::Paused:
        shaka::util::DispatchObjcEvent(_client, @selector(onPlayerPauseEvent:), _shakaPlayer);
        break;
      case shaka::media::VideoPlaybackState::Playing:
        shaka::util::DispatchObjcEvent(_client, @selector(onPlayerPlayingEvent:), _shakaPlayer);
        break;
      case shaka::media::VideoPlaybackState::Ended:
        shaka::util::DispatchObjcEvent(_client, @selector(onPlayerEndedEvent:), _shakaPlayer);
        break;
      default:
        break;
    }
    if (old_state == shaka::media::VideoPlaybackState::Seeking)
      shaka::util::DispatchObjcEvent(_client, @selector(onPlayerSeekedEvent:), _shakaPlayer);
  }

  void OnError(const std::string &error) override {
    OnError(shaka::Error(error));
  }

  void OnSeeking() override {
    shaka::util::DispatchObjcEvent(_client, @selector(onPlayerSeekingEvent:), _shakaPlayer);
  }

  void OnAttachMse() override {
    shaka::util::DispatchObjcEvent(_client, @selector(onPlayerAttachMse:), _shakaPlayer);
  }

  void OnAttachSource() override {
    shaka::util::DispatchObjcEvent(_client, @selector(onPlayerAttachSource:), _shakaPlayer);
  }

  void OnDetach() override {
    shaka::util::DispatchObjcEvent(_client, @selector(onPlayerDetach:), _shakaPlayer);
  }


  void SetClient(id<ShakaPlayerClient> client) {
    _client = client;
  }

  id<ShakaPlayerClient> GetClient() {
    return _client;
  }

  void SetPlayer(ShakaPlayer *shakaPlayer) {
    _shakaPlayer = shakaPlayer;
  }

 private:
  __weak id<ShakaPlayerClient> _client;
  __weak ShakaPlayer *_shakaPlayer;
};

class NetworkFilter final : public shaka::NetworkFilters {
 public:
  NetworkFilter(ShakaPlayer *player, id<ShakaPlayerNetworkFilter> filter)
      : player_(player), filter_(filter) {}

  bool ShouldRemove(id<ShakaPlayerNetworkFilter> filter) {
    return !filter_ || filter_ == filter;
  }

  std::future<shaka::optional<shaka::Error>> OnRequestFilter(shaka::RequestType type,
                                                             shaka::Request *request) override {
    // Create strong references here so if this object is destroyed, these callbacks still work.
    id<ShakaPlayerNetworkFilter> filter = filter_;
    ShakaPlayer *player = player_;
    if (filter && [filter respondsToSelector:@selector(onPlayer:
                                                 networkRequest:ofType:withBlock:)]) {
      auto promise = std::make_shared<promise_type>();
      dispatch_async(dispatch_get_main_queue(), ^{
        auto *objc = [[ShakaPlayerRequest alloc] initWithRequest:*request];
        [filter onPlayer:player
            networkRequest:objc
                    ofType:static_cast<ShakaPlayerRequestType>(type)
                 withBlock:MakeBlock(promise, objc, request)];
      });
      return promise->get_future();
    } else {
      return {};
    }
  }

  std::future<shaka::optional<shaka::Error>> OnResponseFilter(shaka::RequestType type,
                                                              shaka::Response *response) override {
    // Create strong references here so if this object is destroyed, these callbacks still work.
    id<ShakaPlayerNetworkFilter> filter = filter_;
    ShakaPlayer *player = player_;
    if (filter && [filter respondsToSelector:@selector(onPlayer:
                                                 networkResponse:ofType:withBlock:)]) {
      auto promise = std::make_shared<promise_type>();
      dispatch_async(dispatch_get_main_queue(), ^{
        auto *objc = [[ShakaPlayerResponse alloc] initWithResponse:*response];
        [filter onPlayer:player
            networkResponse:objc
                     ofType:static_cast<ShakaPlayerRequestType>(type)
                  withBlock:MakeBlock(promise, objc, response)];
      });
      return promise->get_future();
    } else {
      return {};
    }
  }

 private:
  using promise_type = std::promise<shaka::optional<shaka::Error>>;

  template <typename Objc, typename Cpp>
  static ShakaPlayerAsyncBlock MakeBlock(std::shared_ptr<promise_type> promise, Objc objc,
                                         Cpp cpp) {
    return ^(ShakaPlayerError *error) {
      if (error) {
        promise->set_value(
            shaka::Error(error.severity, error.category, error.code, error.message.UTF8String));
      } else {
        [objc finalize:cpp];  // Propagate changes to the C++ object.
        promise->set_value({});
      }
    };
  }

  __weak ShakaPlayer *player_;
  __weak id<ShakaPlayerNetworkFilter> filter_;
};

}  // namespace

@implementation ShakaPlayerUiInfo

@synthesize paused;
@synthesize ended;
@synthesize seeking;
@synthesize duration;
@synthesize playbackRate;
@synthesize currentTime;
@synthesize volume;
@synthesize muted;
@synthesize isAudioOnly;
@synthesize isLive;
@synthesize closedCaptions;
@synthesize seekRange;
@synthesize bufferedInfo;

@end

NSString *ShakaPlayerLicenseServerConfig(const NSString *key_system) {
  const std::string ret = shaka::LicenseServerConfig(key_system.UTF8String);
  return shaka::util::ObjcConverter<std::string>::ToObjc(ret);
}

NSString *ShakaPlayerAdvancedDrmConfig(const NSString *key_system, const NSString *config) {
  const std::string ret = shaka::AdvancedDrmConfig(key_system.UTF8String, config.UTF8String);
  return shaka::util::ObjcConverter<std::string>::ToObjc(ret);
}

std::shared_ptr<shaka::JsManager> ShakaGetGlobalEngine() {
  std::shared_ptr<shaka::JsManager> ret = gJsEngine.lock();
  if (!ret) {
    shaka::JsManager::StartupOptions options;
    options.static_data_dir = "Frameworks/ShakaPlayerEmbedded.framework";
    options.dynamic_data_dir = std::string(getenv("HOME")) + "/Library";
    options.is_static_relative_to_bundle = true;
    ret.reset(new shaka::JsManager(options));
    gJsEngine = ret;
  }
  return ret;
}

@interface ShakaPlayer () {
  NativeClient _client;
  std::list<NetworkFilter> _filters;
  std::shared_ptr<shaka::JsManager> _engine;

  shaka::media::ios::IosVideoRenderer _video_renderer;
  std::unique_ptr<shaka::media::AudioRenderer> _audio_renderer;

  std::unique_ptr<shaka::media::DefaultMediaPlayer> _media_player;
  std::unique_ptr<shaka::Player> _player;
}

@end

@implementation ShakaPlayer

// MARK: setup

- (instancetype)initWithError:(NSError *__autoreleasing *)error {
  if ((self = [super init])) {
    // Create JS objects.
    _engine = ShakaGetGlobalEngine();
#ifdef SHAKA_SDL_AUDIO
    _audio_renderer.reset(new shaka::media::SdlAudioRenderer(""));
#else
    _audio_renderer.reset(new shaka::media::AppleAudioRenderer());
#endif
    _media_player.reset(
        new shaka::media::DefaultMediaPlayer(&_video_renderer, _audio_renderer.get()));
    _media_player->AddClient(&_client);

    // Set up player.
    _player.reset(new shaka::Player(_engine.get()));
    const auto initResults = _player->Initialize(&_client, _media_player.get());
    if (initResults.has_error()) {
      if (error) {
        *error = [[ShakaPlayerError alloc] initWithError:initResults.error()];
      } else {
        LOG(ERROR) << "Error creating player: " << initResults.error().message;
      }
      return nil;
    }
    _client.SetPlayer(self);
  }
  return self;
}

- (void)setClient:(id<ShakaPlayerClient>)client {
  _client.SetClient(client);
}

- (id<ShakaPlayerClient>)client {
  return _client.GetClient();
}

// MARK: controls

- (void)play {
  _media_player->Play();
}

- (void)pause {
  _media_player->Pause();
}

- (BOOL)paused {
  auto state = _media_player->PlaybackState();
  return state == shaka::media::VideoPlaybackState::Initializing ||
         state == shaka::media::VideoPlaybackState::Paused ||
         state == shaka::media::VideoPlaybackState::Ended;
}

- (BOOL)ended {
  return _media_player->PlaybackState() == shaka::media::VideoPlaybackState::Ended;
}

- (BOOL)seeking {
  return _media_player->PlaybackState() == shaka::media::VideoPlaybackState::Seeking;
}

- (double)duration {
  return _media_player->Duration();
}

- (double)playbackRate {
  return _media_player->PlaybackRate();
}

- (void)setPlaybackRate:(double)rate {
  _media_player->SetPlaybackRate(rate);
}

- (double)currentTime {
  return _media_player->CurrentTime();
}

- (void)setCurrentTime:(double)time {
  _media_player->SetCurrentTime(time);
}

- (double)volume {
  return _media_player->Volume();
}

- (void)setVolume:(double)volume {
  _media_player->SetVolume(volume);
}

- (BOOL)muted {
  return _media_player->Muted();
}

- (void)setMuted:(BOOL)muted {
  _media_player->SetMuted(muted);
}


- (ShakaPlayerLogLevel)logLevel {
  const auto results = shaka::Player::GetLogLevel(_engine.get());
  if (results.has_error())
    return ShakaPlayerLogLevelNone;
  else
    return static_cast<ShakaPlayerLogLevel>(results.results());
}

- (void)setLogLevel:(ShakaPlayerLogLevel)logLevel {
  auto castedLogLevel = static_cast<shaka::Player::LogLevel>(logLevel);
  const auto results = shaka::Player::SetLogLevel(_engine.get(), castedLogLevel);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (NSString *)playerVersion {
  const auto results = shaka::Player::GetPlayerVersion(_engine.get());
  if (results.has_error())
    return @"unknown version";
  else
    return [NSString stringWithCString:results.results().c_str()];
}


- (BOOL)isAudioOnly {
  auto results = _player->IsAudioOnly();
  if (results.has_error())
    return false;
  else
    return results.results();
}

- (BOOL)isLive {
  auto results = _player->IsLive();
  if (results.has_error())
    return false;
  else
    return results.results();
}

- (BOOL)closedCaptions {
  auto results = _player->IsTextTrackVisible();
  if (results.has_error())
    return false;
  else
    return results.results();
}

- (void)setClosedCaptions:(BOOL)closedCaptions {
  _player->SetTextTrackVisibility(closedCaptions);
}

- (AVPlayer *)avPlayer {
  return static_cast<AVPlayer *>(CFBridgingRelease(_media_player->GetAvPlayer()));
}

- (void)getUiInfoWithBlock:(void (^)(ShakaPlayerUiInfo *))block {
  // Use a weak capture to avoid having this callback keep the player alive.
  __weak ShakaPlayer *weakSelf = self;
  auto callback = [weakSelf, block]() {
    ShakaPlayer *player = weakSelf;
    if (!player)
      return;

#define GET_VALUE(field, method)                                            \
  do {                                                                      \
    auto results = player->_player->method();                               \
    if (!results.has_error()) {                                             \
      using T = typename std::decay<decltype(results.results())>::type;     \
      ret.field = shaka::util::ObjcConverter<T>::ToObjc(results.results()); \
    }                                                                       \
  } while (false)
    auto ret = [[ShakaPlayerUiInfo alloc] init];
    ret.paused = [player paused];
    ret.ended = [player ended];
    ret.seeking = [player seeking];
    ret.duration = player->_media_player->Duration();
    ret.playbackRate = player->_media_player->PlaybackRate();
    ret.currentTime = player->_media_player->CurrentTime();
    ret.volume = player->_media_player->Volume();
    ret.muted = player->_media_player->Muted();
    // This callback is run on the JS main thread, so all these should complete
    // synchronously.
    GET_VALUE(isAudioOnly, IsAudioOnly);
    GET_VALUE(isLive, IsLive);
    GET_VALUE(closedCaptions, IsTextTrackVisible);
    GET_VALUE(seekRange, SeekRange);
    GET_VALUE(bufferedInfo, GetBufferedInfo);
    dispatch_async(dispatch_get_main_queue(), ^{
      block(ret);
    });
#undef GET_VALUE
  };
  shaka::JsManagerImpl::Instance()->MainThread()->AddInternalTask(
      shaka::TaskPriority::Internal, "UiInfo", shaka::PlainCallbackTask(std::move(callback)));
}

- (ShakaStats *)getStats {
  auto results = _player->GetStats();
  if (results.has_error()) {
    _client.OnError(results.error());
    return [[ShakaStats alloc] init];
  } else {
    return [[ShakaStats alloc] initWithCpp:results.results()];
  }
}

- (NSArray<ShakaTrack *> *)getTextTracks {
  auto results = _player->GetTextTracks();
  if (results.has_error())
    return [[NSArray<ShakaTrack *> alloc] init];
  else
    return shaka::util::ObjcConverter<std::vector<shaka::Track>>::ToObjc(results.results());
}

- (NSArray<ShakaTrack *> *)getVariantTracks {
  auto results = _player->GetVariantTracks();
  if (results.has_error())
    return [[NSArray<ShakaTrack *> alloc] init];
  else
    return shaka::util::ObjcConverter<std::vector<shaka::Track>>::ToObjc(results.results());
}


- (void)load:(NSString *)uri {
  return [self load:uri withStartTime:NAN];
}

- (void)load:(NSString *)uri withStartTime:(double)startTime {
  @synchronized(self) {
    const auto loadResults = _player->Load(uri.UTF8String, startTime);
    if (loadResults.has_error()) {
      _client.OnError(loadResults.error());
    }
  }
}

- (void)load:(NSString *)uri withBlock:(ShakaPlayerAsyncBlock)block {
  [self load:uri withStartTime:NAN andBlock:block];
}

- (void)load:(NSString *)uri withStartTime:(double)startTime andBlock:(ShakaPlayerAsyncBlock)block {
  auto loadResults = _player->Load(uri.UTF8String, startTime);
  shaka::util::CallBlockForFuture(self, std::move(loadResults), ^(ShakaPlayerError *error) {
    block(error);
  });
}

- (void)unloadWithBlock:(ShakaPlayerAsyncBlock)block {
  auto unloadResults = _player->Unload();
  shaka::util::CallBlockForFuture(self, std::move(unloadResults), block);
}

- (void)configure:(const NSString *)namePath withBool:(BOOL)value {
  _player->Configure(namePath.UTF8String, static_cast<bool>(value));
}

- (void)configure:(const NSString *)namePath withDouble:(double)value {
  _player->Configure(namePath.UTF8String, value);
}

- (void)configure:(const NSString *)namePath withString:(const NSString *)value {
  _player->Configure(namePath.UTF8String, value.UTF8String);
}

- (void)configure:(const NSString *)namePath withData:(NSData *)value {
  _player->Configure(namePath.UTF8String, reinterpret_cast<const uint8_t *>([value bytes]),
                     [value length]);
}

- (void)configureWithDefault:(const NSString *)namePath {
  _player->Configure(namePath.UTF8String, shaka::DefaultValue);
}

- (BOOL)getConfigurationBool:(const NSString *)namePath {
  auto results = _player->GetConfigurationBool(namePath.UTF8String);
  if (results.has_error()) {
    return NO;
  } else {
    return results.results();
  }
}

- (double)getConfigurationDouble:(const NSString *)namePath {
  auto results = _player->GetConfigurationDouble(namePath.UTF8String);
  if (results.has_error()) {
    return 0;
  } else {
    return results.results();
  }
}

- (NSString *)getConfigurationString:(const NSString *)namePath {
  auto results = _player->GetConfigurationString(namePath.UTF8String);
  if (results.has_error()) {
    return @"";
  } else {
    std::string cppString = results.results();
    return [NSString stringWithCString:cppString.c_str()];
  }
}


- (NSArray<ShakaLanguageRole *> *)audioLanguagesAndRoles {
  auto results = _player->GetAudioLanguagesAndRoles();
  if (results.has_error())
    return [[NSArray<ShakaLanguageRole *> alloc] init];
  else
    return shaka::util::ObjcConverter<std::vector<shaka::LanguageRole>>::ToObjc(results.results());
}

- (NSArray<ShakaLanguageRole *> *)textLanguagesAndRoles {
  auto results = _player->GetTextLanguagesAndRoles();
  if (results.has_error())
    return [[NSArray<ShakaLanguageRole *> alloc] init];
  else
    return shaka::util::ObjcConverter<std::vector<shaka::LanguageRole>>::ToObjc(results.results());
}

- (void)selectAudioLanguage:(NSString *)language withRole:(NSString *)role {
  auto results = _player->SelectAudioLanguage(language.UTF8String, role.UTF8String);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)selectAudioLanguage:(NSString *)language {
  auto results = _player->SelectAudioLanguage(language.UTF8String);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)selectTextLanguage:(NSString *)language withRole:(NSString *)role {
  auto results = _player->SelectTextLanguage(language.UTF8String, role.UTF8String);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)selectTextLanguage:(NSString *)language {
  auto results = _player->SelectTextLanguage(language.UTF8String);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)selectTextTrack:(const ShakaTrack *)track {
  auto results = _player->SelectTextTrack([track toCpp]);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)selectVariantTrack:(const ShakaTrack *)track {
  auto results = _player->SelectVariantTrack([track toCpp]);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)selectVariantTrack:(const ShakaTrack *)track withClearBuffer:(BOOL)clear {
  auto results = _player->SelectVariantTrack([track toCpp], clear);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)destroy {
  if (_player)
    _player->Destroy();
}

- (void)addTextTrack:(NSString *)uri
            language:(NSString *)lang
                kind:(NSString *)kind
                mime:(NSString *)mime {
  [self addTextTrack:uri language:lang kind:kind mime:mime codec:@"" label:@""];
}

- (void)addTextTrack:(NSString *)uri
            language:(NSString *)lang
                kind:(NSString *)kind
                mime:(NSString *)mime
               codec:(NSString *)codec {
  [self addTextTrack:uri language:lang kind:kind mime:mime codec:codec label:@""];
}

- (void)addTextTrack:(NSString *)uri
            language:(NSString *)lang
                kind:(NSString *)kind
                mime:(NSString *)mime
               codec:(NSString *)codec
               label:(NSString *)label {
  auto results = _player->AddTextTrack(uri.UTF8String, lang.UTF8String, kind.UTF8String,
                                       mime.UTF8String, codec.UTF8String, label.UTF8String);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)addNetworkFilter:(id<ShakaPlayerNetworkFilter>)filter {
  @synchronized(self) {
    _filters.emplace_back(self, filter);
    _player->AddNetworkFilters(&_filters.back());
  }
}

- (void)removeNetworkFilter:(id<ShakaPlayerNetworkFilter>)filter {
  @synchronized(self) {
    for (auto it = _filters.begin(); it != _filters.end();) {
      if (it->ShouldRemove(filter)) {
        _player->RemoveNetworkFilters(&*it);
        it = _filters.erase(it);
      } else {
        it++;
      }
    }
  }
}

// MARK: +Internal
- (shaka::Player *)playerInstance {
  return _player.get();
}

- (shaka::media::DefaultMediaPlayer *)mediaPlayer {
  return _media_player.get();
}

- (shaka::media::ios::IosVideoRenderer *)videoRenderer {
  return &_video_renderer;
}

@end
