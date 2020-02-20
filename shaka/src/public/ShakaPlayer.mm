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

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "shaka/js_manager.h"
#include "shaka/media/default_media_player.h"
#include "shaka/media/sdl_audio_renderer.h"
#include "shaka/player.h"
#include "shaka/utils.h"

#include "src/js/manifest+Internal.h"
#include "src/js/player_externs+Internal.h"
#include "src/js/stats+Internal.h"
#include "src/js/track+Internal.h"
#include "src/media/ios/ios_video_renderer.h"
#include "src/public/ShakaPlayer+Internal.h"
#include "src/public/error_objc+Internal.h"
#include "src/util/macros.h"
#include "src/util/objc_utils.h"
#import "ShakaPlayer.h"


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
    shaka::util::DispatchObjcEvent(_client, @selector(onPlayer:bufferingChange:), _shakaPlayer, is_buffering);
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

  void SetPlayer(ShakaPlayer *shakaPlayer) {
    _shakaPlayer = shakaPlayer;
  }

 private:
  __weak id<ShakaPlayerClient> _client;
  __weak ShakaPlayer *_shakaPlayer;
};

}  // namespace

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
  std::shared_ptr<shaka::JsManager> _engine;

  shaka::media::ios::IosVideoRenderer _video_renderer;
  std::unique_ptr<shaka::media::SdlAudioRenderer> _audio_renderer;

  std::unique_ptr<shaka::media::DefaultMediaPlayer> _media_player;
  std::unique_ptr<shaka::Player> _player;
}

@end

@implementation ShakaPlayer

// MARK: setup

- (instancetype)initWithClient:(id<ShakaPlayerClient>)client {
  if ((self = [super init])) {
    if (![self setClient:client])
      return nil;
  }
  return self;
}

- (BOOL)setClient:(id<ShakaPlayerClient>)client {
  _client.SetPlayer(self);
  _client.SetClient(client);
  if (_engine) {
    return YES;
  }

  // Create JS objects.
  _engine = ShakaGetGlobalEngine();
  _audio_renderer.reset(new shaka::media::SdlAudioRenderer(""));
  _media_player.reset(
      new shaka::media::DefaultMediaPlayer(&_video_renderer, _audio_renderer.get()));
  _media_player->AddClient(&_client);

  // Set up player.
  _player.reset(new shaka::Player(_engine.get()));
  const auto initResults = _player->Initialize(&_client, _media_player.get());
  if (initResults.has_error()) {
    _client.OnError(initResults.error());
    return NO;
  }
  return YES;
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

- (ShakaBufferedRange *)seekRange {
  auto results = _player->SeekRange();
  if (results.has_error()) {
    _client.OnError(results.error());
    return [[ShakaBufferedRange alloc] init];
  } else {
    return [[ShakaBufferedRange alloc] initWithCpp:results.results()];
  }
}

- (ShakaBufferedInfo *)bufferedInfo {
  auto results = _player->GetBufferedInfo();
  if (results.has_error()) {
    _client.OnError(results.error());
    return [[ShakaBufferedInfo alloc] init];
  } else {
    return [[ShakaBufferedInfo alloc] initWithCpp:results.results()];
  }
}

- (AVPlayer *)avPlayer {
  return static_cast<AVPlayer *>(CFBridgingRelease(_media_player->GetAvPlayer()));
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
