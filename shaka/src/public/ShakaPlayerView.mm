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

#import "shaka/ShakaPlayerView.h"

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
#include "src/public/ShakaPlayerView+Internal.h"
#include "src/public/error_objc+Internal.h"
#include "src/util/macros.h"
#include "src/util/objc_utils.h"

#define ShakaRenderLoopDelay (1.0 / 60)


namespace {

BEGIN_ALLOW_COMPLEX_STATICS
static std::weak_ptr<shaka::JsManager> gJsEngine;
END_ALLOW_COMPLEX_STATICS

class NativeClient final : public shaka::Player::Client, public shaka::media::MediaPlayer::Client {
 public:
  NativeClient() {}

  void OnError(const shaka::Error &error) override {
    ShakaPlayerError *objc_error = [[ShakaPlayerError alloc] initWithError:error];
    shaka::util::DispatchObjcEvent(_client, @selector(onPlayerError:), objc_error);
  }

  void OnBuffering(bool is_buffering) override {
    shaka::util::DispatchObjcEvent(_client, @selector(onPlayerBufferingChange:), is_buffering);
  }


  void OnReadyStateChanged(shaka::media::VideoReadyState old_state,
                           shaka::media::VideoReadyState new_state) override {}

  void OnPlaybackStateChanged(shaka::media::VideoPlaybackState old_state,
                              shaka::media::VideoPlaybackState new_state) override {
    switch (new_state) {
      case shaka::media::VideoPlaybackState::Paused:
        shaka::util::DispatchObjcEvent(_client, @selector(onPlayerPauseEvent));
        break;
      case shaka::media::VideoPlaybackState::Playing:
        shaka::util::DispatchObjcEvent(_client, @selector(onPlayerPlayingEvent));
        break;
      case shaka::media::VideoPlaybackState::Ended:
        shaka::util::DispatchObjcEvent(_client, @selector(onPlayerEndedEvent));
        break;
      default:
        break;
    }
    if (old_state == shaka::media::VideoPlaybackState::Seeking)
      shaka::util::DispatchObjcEvent(_client, @selector(onPlayerSeekedEvent));
  }

  void OnError(const std::string &error) override {
    OnError(shaka::Error(error));
  }

  void OnPlay() override {}

  void OnSeeking() override {
    shaka::util::DispatchObjcEvent(_client, @selector(onPlayerSeekingEvent));
  }

  void OnWaitingForKey() override {}


  void SetClient(id<ShakaPlayerClient> client) {
    _client = client;
  }

 private:
  __weak id<ShakaPlayerClient> _client;
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

@interface ShakaPlayerView () {
  NativeClient _client;
  shaka::media::ios::IosVideoRenderer _video_renderer;
  shaka::media::SdlAudioRenderer *_audio_renderer;

  shaka::media::DefaultMediaPlayer *_media_player;
  shaka::Player *_player;
  NSTimer *_renderLoopTimer;
  NSTimer *_textLoopTimer;
  CALayer *_imageLayer;
  CALayer *_textLayer;

  std::shared_ptr<shaka::JsManager> _engine;
  NSMutableDictionary<NSValue *, NSSet<CALayer *> *> *_cues;
}

@end

@implementation ShakaPlayerView

// MARK: setup

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    if (![self setup])
      return nil;
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder *)aDecoder {
  if ((self = [super initWithCoder:aDecoder])) {
    if (![self setup])
      return nil;
  }
  return self;
}

- (instancetype)initWithClient:(id<ShakaPlayerClient>)client {
  if ((self = [super init])) {
    // super.init calls self.initWithFrame, so this doesn't need to setup again.
    if (![self setClient:client])
      return nil;
  }
  return self;
}

- (void)dealloc {
  delete _player;
  delete _media_player;
  delete _audio_renderer;
}

- (BOOL)setup {
  // Set up the image layer.
  _imageLayer = [CALayer layer];
  [self.layer addSublayer:_imageLayer];

  // Set up the text layer.
  _textLayer = [CALayer layer];
  [self.layer addSublayer:_textLayer];

  // Disable default animations.
  _imageLayer.actions = @{@"position": [NSNull null], @"bounds": [NSNull null]};

  _cues = [[NSMutableDictionary alloc] init];

  return YES;
}

- (BOOL)setClient:(id<ShakaPlayerClient>)client {
  _client.SetClient(client);
  if (_engine) {
    return YES;
  }

  // Create JS objects.
  _engine = ShakaGetGlobalEngine();
  _audio_renderer = new shaka::media::SdlAudioRenderer("");
  _media_player = new shaka::media::DefaultMediaPlayer(&_video_renderer, _audio_renderer);
  _media_player->AddClient(&_client);

  // Set up player.
  _player = new shaka::Player(_engine.get());
  const auto initResults = _player->Initialize(&_client, _media_player);
  if (initResults.has_error()) {
    _client.OnError(initResults.error());
    return NO;
  }
  return YES;
}

- (void)setVideoGravity:(AVLayerVideoGravity)videoGravity {
  if (videoGravity == AVLayerVideoGravityResize) {
    _media_player->SetVideoFillMode(shaka::media::VideoFillMode::Stretch);
  } else if (videoGravity == AVLayerVideoGravityResizeAspectFill) {
    _media_player->SetVideoFillMode(shaka::media::VideoFillMode::Zoom);
  } else if (videoGravity == AVLayerVideoGravityResizeAspect) {
    _media_player->SetVideoFillMode(shaka::media::VideoFillMode::MaintainRatio);
  } else {
    [NSException raise:NSGenericException format:@"Invalid value for videoGravity"];
  }
}

- (void)checkInitialized {
  if (!_engine) {
    [NSException raise:NSGenericException format:@"Must call setClient to initialize the object"];
  }
}

// MARK: rendering

- (void)renderLoop:(NSTimer *)timer {
  @synchronized(self) {
    if (CGImageRef image = _video_renderer.Render()) {
      _imageLayer.contents = (__bridge_transfer id)image;

      // Fit image in frame.
      shaka::ShakaRect image_bounds = {0, 0, CGImageGetWidth(image), CGImageGetHeight(image)};
      shaka::ShakaRect dest_bounds = {
          0,
          0,
          static_cast<int>(self.bounds.size.width),
          static_cast<int>(self.bounds.size.height),
      };
      shaka::ShakaRect src;
      shaka::ShakaRect dest;
      shaka::FitVideoToRegion(image_bounds, dest_bounds, _video_renderer.fill_mode(), &src, &dest);
      _imageLayer.contentsRect = CGRectMake(static_cast<CGFloat>(src.x) / image_bounds.w,
                                            static_cast<CGFloat>(src.y) / image_bounds.h,
                                            static_cast<CGFloat>(src.w) / image_bounds.w,
                                            static_cast<CGFloat>(src.h) / image_bounds.h);
      _imageLayer.frame = CGRectMake(dest.x, dest.y, dest.w, dest.h);
    }
  }
}

- (void)textLoop:(NSTimer *)timer {
  if (self.closedCaptions) {
    BOOL sizeChanged = _textLayer.frame.size.width != _imageLayer.frame.size.width ||
                       _textLayer.frame.size.height != _imageLayer.frame.size.height;

    _textLayer.hidden = NO;
    _textLayer.frame = _imageLayer.frame;

    [self remakeTextCues:sizeChanged];
  } else {
    _textLayer.hidden = YES;
  }
}

- (void)remakeTextCues:(BOOL)sizeChanged {
  auto text_tracks = _media_player->TextTracks();
  auto activeCues = text_tracks[0]->active_cues(self.currentTime);

  if (sizeChanged) {
    for (CALayer *layer in [_textLayer.sublayers copy])
      [layer removeFromSuperlayer];
    [_cues removeAllObjects];
  }

  // Use the system font for body. The ensures that if the user changes their font size, it will use
  // that font size.
  UIFont *font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];

  // Add layers for new cues.
  for (unsigned long i = 0; i < activeCues.size(); i++) {
    // Read the cues in inverse order, so the oldest cue is at the bottom.
    std::shared_ptr<shaka::media::VTTCue> cue = activeCues[activeCues.size() - i - 1];
    if (_cues[[NSValue valueWithPointer:cue.get()]] != nil)
      continue;

    NSString *cueText = [NSString stringWithUTF8String:cue->text().c_str()];
    NSMutableSet<CALayer *> *layers = [[NSMutableSet alloc] init];
    for (NSString *line in [cueText componentsSeparatedByString:@"\n"]) {
      CATextLayer *cueLayer = [[CATextLayer alloc] init];
      cueLayer.string = line;
      cueLayer.font = CGFontCreateWithFontName(static_cast<CFStringRef>(font.fontName));
      cueLayer.fontSize = font.pointSize;
      cueLayer.backgroundColor = [UIColor blackColor].CGColor;
      cueLayer.foregroundColor = [UIColor whiteColor].CGColor;
      cueLayer.alignmentMode = kCAAlignmentCenter;

      // TODO: Take into account direction setting, snap to lines, position align, etc.

      // Determine size of cue line.
      CGFloat width = static_cast<CGFloat>(cue->size() * (_textLayer.bounds.size.width) * 0.01);
      NSStringDrawingOptions options = NSStringDrawingUsesLineFragmentOrigin;
      // TODO: Sometimes, if the system font size is set high, this will make very wrong estimates.
      CGSize effectiveSize = [line boundingRectWithSize:CGSizeMake(width, 9999)
                                                options:options
                                             attributes:@{NSFontAttributeName: font}
                                                context:nil]
                                 .size;
      // The size estimates don't always seem to be 100% accurate, so this adds a bit to the width.
      width = ceil(effectiveSize.width) + 16;
      CGFloat height = ceil(effectiveSize.height);
      CGFloat x = (_textLayer.bounds.size.width - width) * 0.5f;
      cueLayer.frame = CGRectMake(x, 0, width, height);

      [_textLayer addSublayer:cueLayer];
      [layers addObject:cueLayer];
    }
    [_cues setObject:layers forKey:[NSValue valueWithPointer:cue.get()]];
  }

  // Remove any existing cues that aren't active anymore.
  std::unordered_set<shaka::media::VTTCue *> activeCuesSet;
  for (auto &cue : activeCues)
    activeCuesSet.insert(cue.get());
  NSMutableSet<CALayer *> *expectedLayers = [[NSMutableSet alloc] init];
  for (NSValue *cue in [_cues allKeys]) {
    if (activeCuesSet.count(reinterpret_cast<shaka::media::VTTCue *>([cue pointerValue])) == 0) {
      // Since the layers aren't added to "expectedLayers", they will be removed below.
      [_cues removeObjectForKey:cue];
    } else {
      NSSet<CALayer *> *layers = _cues[cue];
      [expectedLayers unionSet:layers];
    }
  }

  // Remove any layers in the view that aren't associated with any cues.
  for (CALayer *layer in [_textLayer.sublayers copy]) {
    if (![expectedLayers containsObject:layer])
      [layer removeFromSuperlayer];
  }

  // Move all the remaining layers to appear at the bottom of the screen.
  CGFloat y = _textLayer.bounds.size.height;
  for (auto i = _textLayer.sublayers.count; i > 0; i--) {
    // Read the cue layers in inverse order, since they are being drawn from the bottom up.
    CALayer *cueLayer = _textLayer.sublayers[i - 1];

    CGSize size = cueLayer.frame.size;
    y -= size.height;
    cueLayer.frame = CGRectMake(cueLayer.frame.origin.x, y, size.width, size.height);
  }
}

// MARK: controls

- (void)play {
  [self checkInitialized];
  _media_player->Play();
}

- (void)pause {
  [self checkInitialized];
  _media_player->Pause();
}

- (BOOL)paused {
  [self checkInitialized];
  auto state = _media_player->PlaybackState();
  return state == shaka::media::VideoPlaybackState::Initializing ||
         state == shaka::media::VideoPlaybackState::Paused ||
         state == shaka::media::VideoPlaybackState::Ended;
}

- (BOOL)ended {
  [self checkInitialized];
  return _media_player->PlaybackState() == shaka::media::VideoPlaybackState::Ended;
}

- (BOOL)seeking {
  [self checkInitialized];
  return _media_player->PlaybackState() == shaka::media::VideoPlaybackState::Seeking;
}

- (double)duration {
  [self checkInitialized];
  return _media_player->Duration();
}

- (double)playbackRate {
  [self checkInitialized];
  return _media_player->PlaybackRate();
}

- (void)setPlaybackRate:(double)rate {
  [self checkInitialized];
  _media_player->SetPlaybackRate(rate);
}

- (double)currentTime {
  [self checkInitialized];
  return _media_player->CurrentTime();
}

- (void)setCurrentTime:(double)time {
  [self checkInitialized];
  _media_player->SetCurrentTime(time);
}

- (double)volume {
  [self checkInitialized];
  return _media_player->Volume();
}

- (void)setVolume:(double)volume {
  [self checkInitialized];
  _media_player->SetVolume(volume);
}

- (BOOL)muted {
  [self checkInitialized];
  return _media_player->Muted();
}

- (void)setMuted:(BOOL)muted {
  [self checkInitialized];
  _media_player->SetMuted(muted);
}


- (ShakaPlayerLogLevel)logLevel {
  [self checkInitialized];
  const auto results = shaka::Player::GetLogLevel(_engine.get());
  if (results.has_error())
    return ShakaPlayerLogLevelNone;
  else
    return static_cast<ShakaPlayerLogLevel>(results.results());
}

- (void)setLogLevel:(ShakaPlayerLogLevel)logLevel {
  [self checkInitialized];
  auto castedLogLevel = static_cast<shaka::Player::LogLevel>(logLevel);
  const auto results = shaka::Player::SetLogLevel(_engine.get(), castedLogLevel);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (NSString *)playerVersion {
  [self checkInitialized];
  const auto results = shaka::Player::GetPlayerVersion(_engine.get());
  if (results.has_error())
    return @"unknown version";
  else
    return [NSString stringWithCString:results.results().c_str()];
}


- (BOOL)isAudioOnly {
  [self checkInitialized];
  auto results = _player->IsAudioOnly();
  if (results.has_error())
    return false;
  else
    return results.results();
}

- (BOOL)isLive {
  [self checkInitialized];
  auto results = _player->IsLive();
  if (results.has_error())
    return false;
  else
    return results.results();
}

- (BOOL)closedCaptions {
  [self checkInitialized];
  auto results = _player->IsTextTrackVisible();
  if (results.has_error())
    return false;
  else
    return results.results();
}

- (void)setClosedCaptions:(BOOL)closedCaptions {
  [self checkInitialized];
  _player->SetTextTrackVisibility(closedCaptions);
}

- (ShakaBufferedRange *)seekRange {
  [self checkInitialized];
  auto results = _player->SeekRange();
  if (results.has_error()) {
    _client.OnError(results.error());
    return [[ShakaBufferedRange alloc] init];
  } else {
    return [[ShakaBufferedRange alloc] initWithCpp:results.results()];
  }
}

- (ShakaBufferedInfo *)bufferedInfo {
  [self checkInitialized];
  auto results = _player->GetBufferedInfo();
  if (results.has_error()) {
    _client.OnError(results.error());
    return [[ShakaBufferedInfo alloc] init];
  } else {
    return [[ShakaBufferedInfo alloc] initWithCpp:results.results()];
  }
}


- (ShakaStats *)getStats {
  [self checkInitialized];
  auto results = _player->GetStats();
  if (results.has_error()) {
    _client.OnError(results.error());
    return [[ShakaStats alloc] init];
  } else {
    return [[ShakaStats alloc] initWithCpp:results.results()];
  }
}

- (NSArray<ShakaTrack *> *)getTextTracks {
  [self checkInitialized];
  auto results = _player->GetTextTracks();
  if (results.has_error())
    return [[NSArray<ShakaTrack *> alloc] init];
  else
    return shaka::util::ObjcConverter<std::vector<shaka::Track>>::ToObjc(results.results());
}

- (NSArray<ShakaTrack *> *)getVariantTracks {
  [self checkInitialized];
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
    [self checkInitialized];
    const auto loadResults = _player->Load(uri.UTF8String, startTime);
    if (loadResults.has_error()) {
      _client.OnError(loadResults.error());
    } else {
      [self postLoadOperations];
    }
  }
}

- (void)load:(NSString *)uri withBlock:(ShakaPlayerAsyncBlock)block {
  [self load:uri withStartTime:NAN andBlock:block];
}

- (void)load:(NSString *)uri withStartTime:(double)startTime andBlock:(ShakaPlayerAsyncBlock)block {
  [self checkInitialized];
  auto loadResults = self->_player->Load(uri.UTF8String, startTime);
  shaka::util::CallBlockForFuture(self, std::move(loadResults), ^(ShakaPlayerError *error) {
    if (!error) {
      [self postLoadOperations];
    }
    block(error);
  });
}

- (void)postLoadOperations {
  _imageLayer.contents = nil;

  // Start up the render loop.
  SEL renderLoopSelector = @selector(renderLoop:);
  _renderLoopTimer = [NSTimer scheduledTimerWithTimeInterval:ShakaRenderLoopDelay
                                                      target:self
                                                    selector:renderLoopSelector
                                                    userInfo:nullptr
                                                     repeats:YES];
  _textLoopTimer = [NSTimer scheduledTimerWithTimeInterval:0.25
                                                    target:self
                                                  selector:@selector(textLoop:)
                                                  userInfo:nullptr
                                                   repeats:YES];
}

- (void)unloadWithBlock:(ShakaPlayerAsyncBlock)block {
  [self checkInitialized];
  // Stop the render loop before acquiring the mutex, to avoid deadlocks.
  [_renderLoopTimer invalidate];
  [_textLoopTimer invalidate];

  self->_imageLayer.contents = nil;
  for (CALayer *layer in [self->_textLayer.sublayers copy])
    [layer removeFromSuperlayer];
  auto unloadResults = self->_player->Unload();
  shaka::util::CallBlockForFuture(self, std::move(unloadResults), block);
}

- (void)configure:(const NSString *)namePath withBool:(BOOL)value {
  [self checkInitialized];
  _player->Configure(namePath.UTF8String, static_cast<bool>(value));
}

- (void)configure:(const NSString *)namePath withDouble:(double)value {
  [self checkInitialized];
  _player->Configure(namePath.UTF8String, value);
}

- (void)configure:(const NSString *)namePath withString:(const NSString *)value {
  [self checkInitialized];
  _player->Configure(namePath.UTF8String, value.UTF8String);
}

- (void)configureWithDefault:(const NSString *)namePath {
  [self checkInitialized];
  _player->Configure(namePath.UTF8String, shaka::DefaultValue);
}

- (BOOL)getConfigurationBool:(const NSString *)namePath {
  [self checkInitialized];
  auto results = _player->GetConfigurationBool(namePath.UTF8String);
  if (results.has_error()) {
    return NO;
  } else {
    return results.results();
  }
}

- (double)getConfigurationDouble:(const NSString *)namePath {
  [self checkInitialized];
  auto results = _player->GetConfigurationDouble(namePath.UTF8String);
  if (results.has_error()) {
    return 0;
  } else {
    return results.results();
  }
}

- (NSString *)getConfigurationString:(const NSString *)namePath {
  [self checkInitialized];
  auto results = _player->GetConfigurationString(namePath.UTF8String);
  if (results.has_error()) {
    return @"";
  } else {
    std::string cppString = results.results();
    return [NSString stringWithCString:cppString.c_str()];
  }
}


- (NSArray<ShakaLanguageRole *> *)audioLanguagesAndRoles {
  [self checkInitialized];
  auto results = _player->GetAudioLanguagesAndRoles();
  if (results.has_error())
    return [[NSArray<ShakaLanguageRole *> alloc] init];
  else
    return shaka::util::ObjcConverter<std::vector<shaka::LanguageRole>>::ToObjc(results.results());
}

- (NSArray<ShakaLanguageRole *> *)textLanguagesAndRoles {
  [self checkInitialized];
  auto results = _player->GetTextLanguagesAndRoles();
  if (results.has_error())
    return [[NSArray<ShakaLanguageRole *> alloc] init];
  else
    return shaka::util::ObjcConverter<std::vector<shaka::LanguageRole>>::ToObjc(results.results());
}

- (void)selectAudioLanguage:(NSString *)language withRole:(NSString *)role {
  [self checkInitialized];
  auto results = _player->SelectAudioLanguage(language.UTF8String, role.UTF8String);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)selectAudioLanguage:(NSString *)language {
  [self checkInitialized];
  auto results = _player->SelectAudioLanguage(language.UTF8String);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)selectTextLanguage:(NSString *)language withRole:(NSString *)role {
  [self checkInitialized];
  auto results = _player->SelectTextLanguage(language.UTF8String, role.UTF8String);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)selectTextLanguage:(NSString *)language {
  [self checkInitialized];
  auto results = _player->SelectTextLanguage(language.UTF8String);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)selectTextTrack:(const ShakaTrack *)track {
  [self checkInitialized];
  auto results = _player->SelectTextTrack([track toCpp]);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)selectVariantTrack:(const ShakaTrack *)track {
  [self checkInitialized];
  auto results = _player->SelectVariantTrack([track toCpp]);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)selectVariantTrack:(const ShakaTrack *)track withClearBuffer:(BOOL)clear {
  [self checkInitialized];
  auto results = _player->SelectVariantTrack([track toCpp], clear);
  if (results.has_error()) {
    _client.OnError(results.error());
  }
}

- (void)destroy {
  if (_player)
    _player->Destroy();
}

// MARK: +Internal
- (shaka::Player *)playerInstance {
  return _player;
}

@end
