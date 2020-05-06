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

#include "src/media/ios/av_media_player.h"

#import <AVFoundation/AVFoundation.h>
#import <AVKit/AVKit.h>
#include <glog/logging.h>

#include <vector>

#include "src/debug/mutex.h"
#include "src/media/ios/av_media_track.h"
#include "src/media/ios/av_text_track.h"
#include "src/util/macros.h"
#include "src/util/utils.h"

#define LISTEN_PLAYER_KEY_PATHS \
  { @"error", @"rate" }
#define LISTEN_ITEM_KEY_PATHS \
  { @"status", @"playbackLikelyToKeepUp", @"playbackBufferEmpty" }

@interface LayerWrapper : CALayer
@end

@interface ValueObserver : NSObject
- (id)initWithImpl:(shaka::media::ios::AvMediaPlayer::Impl *)impl;
@end

class shaka::media::ios::AvMediaPlayer::Impl {
 public:
  Impl(ClientList *clients)
      : mutex_("AvMediaPlayer"),
        layer_([[LayerWrapper alloc] init]),
        clients_(clients),
        load_id_(0),
        old_playback_state_(VideoPlaybackState::Detached),
        old_ready_state_(VideoReadyState::NotAttached),
        requested_play_(false),
        loaded_(false),
        player_(nil),
        player_layer_(nil),
        observer_([[ValueObserver alloc] initWithImpl:this]) {}

  ~Impl() {
    Detach();
  }

  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(Impl);

  /**
   * Atomically returns the current AVPlayer instance.  The value is explicitly retained so
   * another thread can change the player but this thread will affect the returned AVPlayer.
   * Because of ARC, this will be allowed, but not be visible since the video was removed.
   */
  AVPlayer *GetPlayer() __attribute__((ns_returns_retained)) {
    util::shared_lock<SharedMutex> lock(mutex_);
    return player_;
  }

  const void *GetIosView() {
    return CFBridgingRetain(layer_);
  }

  const void *GetAvPlayerAsPointer() {
    util::shared_lock<SharedMutex> lock(mutex_);
    return CFBridgingRetain(player_);
  }

  void AddClient(MediaPlayer::Client *client) {
    clients_->AddClient(client);
  }

  void RemoveClient(MediaPlayer::Client *client) {
    clients_->RemoveClient(client);
  }


  bool SetVideoFillMode(VideoFillMode mode) {
    util::shared_lock<SharedMutex> lock(mutex_);
    if (!player_layer_)
      return false;

    switch (mode) {
      case VideoFillMode::Stretch:
        player_layer_.videoGravity = AVLayerVideoGravityResize;
        break;
      case VideoFillMode::Zoom:
        player_layer_.videoGravity = AVLayerVideoGravityResizeAspectFill;
        break;
      case VideoFillMode::MaintainRatio:
        player_layer_.videoGravity = AVLayerVideoGravityResizeAspect;
        break;
      default:
        return false;
    }
    return true;
  }

  void Play() {
    AVPlayer *player = GetPlayer();
    if (player) {
      [player play];

      std::unique_lock<SharedMutex> lock(mutex_);
      requested_play_ = true;
    }
  }

  void Pause() {
    AVPlayer *player = GetPlayer();
    if (player) {
      [player pause];

      std::unique_lock<SharedMutex> lock(mutex_);
      requested_play_ = false;
    }
  }

  bool Attach(const std::string &src) {
    std::unique_lock<SharedMutex> lock(mutex_);
    DCHECK(!player_) << "Already attached";

    auto *str = [NSString stringWithUTF8String:src.c_str()];
    if (!str)
      return false;
    auto *url = [NSURL URLWithString:str];
    if (!url)
      return false;
    player_ = [AVPlayer playerWithURL:url];
    if (!player_)
      return false;

    const auto options = NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld;
    for (auto *name : LISTEN_PLAYER_KEY_PATHS)
      [player_ addObserver:observer_ forKeyPath:name options:options context:nil];
    for (auto *name : LISTEN_ITEM_KEY_PATHS)
      [player_.currentItem addObserver:observer_ forKeyPath:name options:options context:nil];

    // Note that this must be done on the main thread for the layer to be visible.  Since this is
    // asynchronous, store the current load ID so we know if another attach/detach happened.  We
    // cannot wait for this to complete since that would block the JS main thread and could
    // deadlock.
    int load_id = ++load_id_;
    dispatch_async(dispatch_get_main_queue(), ^{
      std::unique_lock<SharedMutex> lock(mutex_);
      if (load_id != load_id_)
        return;

      player_layer_ = [AVPlayerLayer playerLayerWithPlayer:player_];
      player_layer_.frame = layer_.frame;
      [layer_ addSublayer:player_layer_];

      clients_->OnAttachSource();
    });

    return true;
  }

  void Detach() {
    std::unique_lock<SharedMutex> lock(mutex_);
    load_id_++;
    if (player_) {
      for (auto *name : LISTEN_PLAYER_KEY_PATHS)
        [player_ removeObserver:observer_ forKeyPath:name];
      for (auto *name : LISTEN_ITEM_KEY_PATHS)
        [player_.currentItem removeObserver:observer_ forKeyPath:name];

      [player_ pause];
      player_ = nil;
    }
    audio_tracks_.clear();
    video_tracks_.clear();
    text_tracks_.clear();
    old_playback_state_ = VideoPlaybackState::Detached;
    old_ready_state_ = VideoReadyState::NotAttached;
    requested_play_ = false;
    loaded_ = false;
    player_layer_ = nil;

    for (CALayer *layer in [layer_.sublayers copy])
      [layer removeFromSuperlayer];

    clients_->OnDetach();
  }

  VideoPlaybackState UpdateAndGetVideoPlaybackState() {
    std::unique_lock<SharedMutex> lock(mutex_);
    const auto new_state = VideoPlaybackState(player_);
    if (old_playback_state_ != new_state) {
      const auto old_state = old_playback_state_;
      old_playback_state_ = new_state;
      clients_->OnPlaybackStateChanged(old_state, new_state);
      if (new_state == VideoPlaybackState::Playing && requested_play_)
        clients_->OnPlay();

      requested_play_ = false;
    }
    return new_state;
  }

  VideoReadyState UpdateAndGetVideoReadyState() {
    std::unique_lock<SharedMutex> lock(mutex_);
    CheckLoaded();
    const auto new_state = VideoReadyState(player_, loaded_);
    if (old_ready_state_ != new_state) {
      const auto old_state = old_ready_state_;
      old_ready_state_ = new_state;
      clients_->OnReadyStateChanged(old_state, new_state);
    }
    return new_state;
  }

  std::vector<std::shared_ptr<MediaTrack>> AudioTracks() {
    std::unique_lock<SharedMutex> lock(mutex_);
    CheckLoaded();
    return audio_tracks_;
  }

  std::vector<std::shared_ptr<MediaTrack>> VideoTracks() {
    std::unique_lock<SharedMutex> lock(mutex_);
    CheckLoaded();
    return video_tracks_;
  }

  std::vector<std::shared_ptr<TextTrack>> TextTracks() {
    std::unique_lock<SharedMutex> lock(mutex_);
    CheckLoaded();
    return text_tracks_;
  }

  void OnError(const std::string &message) {
    clients_->OnError(message);
  }

  void OnPlaybackRateChanged(double old_rate, double new_rate) {
    clients_->OnPlaybackRateChanged(old_rate, new_rate);
  }

 private:
  static VideoPlaybackState VideoPlaybackState(AVPlayer *player) {
    if (!player)
      return VideoPlaybackState::Detached;

    if (player.error)
      return VideoPlaybackState::Errored;

    const double duration = CMTimeGetSeconds(player.currentItem.asset.duration);
    const double time = CMTimeGetSeconds([player currentTime]);
    if (!isnan(duration) && duration > 0 && time + 0.1 >= duration)
      return VideoPlaybackState::Ended;

    if (player.rate == 0)
      return VideoPlaybackState::Paused;
    if (player.currentItem.playbackBufferEmpty)
      return VideoPlaybackState::Buffering;

    return VideoPlaybackState::Playing;
  }

  static VideoReadyState VideoReadyState(AVPlayer *player, bool loaded) {
    if (!player)
      return VideoReadyState::NotAttached;

    if (player.currentItem.status != AVPlayerItemStatusReadyToPlay || !loaded) {
      return VideoReadyState::HaveNothing;
    }
    if (player.currentItem.playbackBufferEmpty) {
      // TODO: This isn't correct.  This will be false if we don't have enough data to move
      // forward.  But we may still be HaveCurrentData.  There isn't an easy way to detect this
      // case, so for now we just use HaveMetaData.
      return VideoReadyState::HaveMetadata;
    }

    if (!player.currentItem.playbackLikelyToKeepUp)
      return VideoReadyState::HaveFutureData;
    return VideoReadyState::HaveEnoughData;
  }

  void CheckLoaded() {
    if (loaded_ || !player_)
      return;
    if (player_.currentItem.tracks.count == 0)
      return;

    // Can't change video tracks directly.  Return an empty list for audio-only content and a
    // dummy value for audio+video content.
    for (AVPlayerItemTrack *track in player_.currentItem.tracks) {
      if ([track.assetTrack.mediaType isEqualToString:AVMediaTypeVideo]) {
        std::shared_ptr<MediaTrack> track(new MediaTrack(MediaTrackKind::Unknown, "", "", ""));
        video_tracks_.emplace_back(track);
        clients_->OnAddVideoTrack(track);
        break;
      }
    }

    auto *item = player_.currentItem;
    auto *group =
        [item.asset mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicAudible];
    if (group) {
      for (AVMediaSelectionOption *option in [group options]) {
        std::shared_ptr<MediaTrack> track(new AvMediaTrack(item, group, option));
        audio_tracks_.emplace_back(track);
        clients_->OnAddAudioTrack(track);
      }
    }

    group = [item.asset mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicLegible];
    if (group) {
      for (AVMediaSelectionOption *option in [group options]) {
        std::shared_ptr<TextTrack> track(new AvTextTrack(item, group, option));
        text_tracks_.emplace_back(track);
        clients_->OnAddTextTrack(track);
      }
    }
    loaded_ = true;
  }

  SharedMutex mutex_;
  LayerWrapper *const layer_;
  ClientList *const clients_;

  std::vector<std::shared_ptr<MediaTrack>> audio_tracks_;
  std::vector<std::shared_ptr<MediaTrack>> video_tracks_;
  std::vector<std::shared_ptr<TextTrack>> text_tracks_;

  int load_id_;
  enum VideoPlaybackState old_playback_state_;
  enum VideoReadyState old_ready_state_;
  bool requested_play_;
  bool loaded_;

  AVPlayer *player_;
  AVPlayerLayer *player_layer_;
  ValueObserver *observer_;
};

@implementation LayerWrapper

+ (Class)layerClass {
  return [LayerWrapper class];
}

- (void)layoutSublayers {
  [super layoutSublayers];

  if (self.superlayer)
    self.frame = self.superlayer.bounds;
  for (CALayer *layer in self.sublayers)
    layer.frame = self.bounds;
}

@end

@implementation ValueObserver {
  shaka::media::ios::AvMediaPlayer::Impl *impl_;
}

- (id)initWithImpl:(shaka::media::ios::AvMediaPlayer::Impl *)impl {
  if (self) {
    impl_ = impl;
  }
  return self;
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id> *)change
                       context:(void *)context {
  id old = [change objectForKey:NSKeyValueChangeOldKey];
  id new_ = [change objectForKey:NSKeyValueChangeNewKey];
  if ([keyPath isEqual:@"error"]) {
    if (!old && new_) {
      auto *error = static_cast<NSError *>(new_);
      std::string str = [[error localizedDescription] UTF8String];
      impl_->OnError(str);
    }
  } else if ([keyPath isEqual:@"rate"]) {
    auto *old_num = static_cast<NSNumber *>(old);
    auto *new_num = static_cast<NSNumber *>(new_);
    impl_->OnPlaybackRateChanged([old_num doubleValue], [new_num doubleValue]);
  }

  impl_->UpdateAndGetVideoPlaybackState();
  impl_->UpdateAndGetVideoReadyState();
}

@end


namespace shaka {
namespace media {
namespace ios {

AvMediaPlayer::AvMediaPlayer(ClientList *clients) : impl_(new Impl(clients)) {}
AvMediaPlayer::~AvMediaPlayer() {}

const void *AvMediaPlayer::GetIosView() {
  return impl_->GetIosView();
}
const void *AvMediaPlayer::GetAvPlayer() {
  return impl_->GetAvPlayerAsPointer();
}

MediaCapabilitiesInfo AvMediaPlayer::DecodingInfo(const MediaDecodingConfiguration &config) const {
  MediaCapabilitiesInfo ret;
  if (config.type == MediaDecodingType::File &&
      (!config.audio.content_type.empty() || !config.video.content_type.empty())) {
    ret.supported = true;
    if (!config.audio.content_type.empty()) {
      auto *str = [NSString stringWithUTF8String:config.audio.content_type.c_str()];
      ret.supported &= [AVURLAsset isPlayableExtendedMIMEType:str];
    }
    if (!config.video.content_type.empty()) {
      auto *str = [NSString stringWithUTF8String:config.video.content_type.c_str()];
      ret.supported &= [AVURLAsset isPlayableExtendedMIMEType:str];
    }
  }
  return ret;
}
VideoPlaybackQuality AvMediaPlayer::VideoPlaybackQuality() const {
  return {};  // Unsupported.
}
void AvMediaPlayer::AddClient(Client *client) const {
  impl_->AddClient(client);
}
void AvMediaPlayer::RemoveClient(Client *client) const {
  impl_->RemoveClient(client);
}

std::vector<BufferedRange> AvMediaPlayer::GetBuffered() const {
  AVPlayer *player = impl_->GetPlayer();

  std::vector<BufferedRange> ret;
  if (player) {
    auto *ranges = player.currentItem.loadedTimeRanges;
    ret.reserve([ranges count]);
    for (size_t i = 0; i < [ranges count]; i++) {
      auto range = [[ranges objectAtIndex:i] CMTimeRangeValue];
      auto start = CMTimeGetSeconds(range.start);
      ret.emplace_back(start, start + CMTimeGetSeconds(range.duration));
    }
  }
  return ret;
}
VideoReadyState AvMediaPlayer::ReadyState() const {
  return impl_->UpdateAndGetVideoReadyState();
}
VideoPlaybackState AvMediaPlayer::PlaybackState() const {
  return impl_->UpdateAndGetVideoPlaybackState();
}
std::vector<std::shared_ptr<MediaTrack>> AvMediaPlayer::AudioTracks() {
  return impl_->AudioTracks();
}
std::vector<std::shared_ptr<const MediaTrack>> AvMediaPlayer::AudioTracks() const {
  auto ret = impl_->AudioTracks();
  return {ret.begin(), ret.end()};
}
std::vector<std::shared_ptr<MediaTrack>> AvMediaPlayer::VideoTracks() {
  return impl_->VideoTracks();
}
std::vector<std::shared_ptr<const MediaTrack>> AvMediaPlayer::VideoTracks() const {
  auto ret = impl_->VideoTracks();
  return {ret.begin(), ret.end()};
}
std::vector<std::shared_ptr<TextTrack>> AvMediaPlayer::TextTracks() {
  return impl_->TextTracks();
}
std::vector<std::shared_ptr<const TextTrack>> AvMediaPlayer::TextTracks() const {
  auto ret = impl_->TextTracks();
  return {ret.begin(), ret.end()};
}
std::shared_ptr<TextTrack> AvMediaPlayer::AddTextTrack(TextTrackKind kind, const std::string &label,
                                                       const std::string &language) {
  LOG(ERROR) << "Cannot add text tracks to src= content";
  return nullptr;
}

bool AvMediaPlayer::SetVideoFillMode(VideoFillMode mode) {
  return impl_->SetVideoFillMode(mode);
}
uint32_t AvMediaPlayer::Width() const {
  AVPlayer *player = impl_->GetPlayer();
  if (player)
    return static_cast<uint32_t>(player.currentItem.presentationSize.width);
  else
    return 0;
}
uint32_t AvMediaPlayer::Height() const {
  AVPlayer *player = impl_->GetPlayer();
  if (player)
    return static_cast<uint32_t>(player.currentItem.presentationSize.height);
  else
    return 0;
}
double AvMediaPlayer::Volume() const {
  AVPlayer *player = impl_->GetPlayer();
  if (player)
    return player.volume;
  else
    return 0;
}
void AvMediaPlayer::SetVolume(double volume) {
  AVPlayer *player = impl_->GetPlayer();
  if (player)
    player.volume = static_cast<float>(volume);
}
bool AvMediaPlayer::Muted() const {
  AVPlayer *player = impl_->GetPlayer();
  if (player)
    return player.muted;
  else
    return false;
}
void AvMediaPlayer::SetMuted(bool muted) {
  AVPlayer *player = impl_->GetPlayer();
  if (player)
    player.muted = muted;
}

void AvMediaPlayer::Play() {
  impl_->Play();
}
void AvMediaPlayer::Pause() {
  impl_->Pause();
}
double AvMediaPlayer::CurrentTime() const {
  AVPlayer *player = impl_->GetPlayer();
  if (player)
    return CMTimeGetSeconds([player currentTime]);
  else
    return 0;
}
void AvMediaPlayer::SetCurrentTime(double time) {
  AVPlayer *player = impl_->GetPlayer();
  if (player)
    [player seekToTime:CMTimeMakeWithSeconds(time, 1000)];
}
double AvMediaPlayer::Duration() const {
  AVPlayer *player = impl_->GetPlayer();
  if (player)
    return CMTimeGetSeconds(player.currentItem.asset.duration);
  else
    return 0;
}
void AvMediaPlayer::SetDuration(double duration) {
  LOG(FATAL) << "Cannot set duration of src= assets";
}
double AvMediaPlayer::PlaybackRate() const {
  AVPlayer *player = impl_->GetPlayer();
  if (player)
    return player.rate;
  else
    return 0;
}
void AvMediaPlayer::SetPlaybackRate(double rate) {
  AVPlayer *player = impl_->GetPlayer();
  if (player)
    player.rate = static_cast<float>(rate);
}

bool AvMediaPlayer::AttachSource(const std::string &src) {
  return impl_->Attach(src);
}
bool AvMediaPlayer::AttachMse() {
  return false;
}
bool AvMediaPlayer::AddMseBuffer(const std::string &mime, bool is_video,
                                 const ElementaryStream *stream) {
  return false;
}
void AvMediaPlayer::LoadedMetaData(double duration) {}
void AvMediaPlayer::MseEndOfStream() {}
bool AvMediaPlayer::SetEmeImplementation(const std::string &key_system,
                                         eme::Implementation *implementation) {
  return false;
}
void AvMediaPlayer::Detach() {
  impl_->Detach();
}

}  // namespace ios
}  // namespace media
}  // namespace shaka
