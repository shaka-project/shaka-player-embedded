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

#ifndef SHAKA_EMBEDDED_JS_MSE_TRACK_LIST_H_
#define SHAKA_EMBEDDED_JS_MSE_TRACK_LIST_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "shaka/media/media_player.h"
#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/debug/mutex.h"
#include "src/js/events/event.h"
#include "src/js/events/event_target.h"
#include "src/mapping/backing_object_factory.h"

namespace shaka {

namespace media {

class MediaTrack;
class TextTrack;

}  // namespace media

namespace js {
namespace mse {

class AudioTrack;
class VideoTrack;
class TextTrack;

/**
 * Stores a list of tracks.  This acts as a proxy between the MediaPlayer track
 * objects and the JavaScript wrappers of them.  Subclasses will need to call
 * AddTrack/RemoveTrack when tracks get added/removed based on the events from
 * the MediaPlayer.
 */
template <typename JsTrack, typename PubTrack>
class TrackList : public events::EventTarget, media::MediaPlayer::Client {
 public:
  explicit TrackList(media::MediaPlayer* player)
      : mutex_("TrackList"), player_(player) {
    player->AddClient(this);
  }

  ~TrackList() override {
    Detach();
  }

  void Trace(memory::HeapTracer* tracer) const override {
    events::EventTarget::Trace(tracer);
    std::unique_lock<Mutex> lock(mutex_);
    for (auto& pair : tracks_)
      tracer->Trace(&pair.second);
  }

  size_t length() const {
    std::unique_lock<Mutex> lock(mutex_);
    return tracks_.size();
  }

  bool GetIndex(size_t i, RefPtr<JsTrack>* track) const {
    std::unique_lock<Mutex> lock(mutex_);
    if (i >= tracks_.size())
      return false;

    *track = tracks_[i].second;
    return true;
  }


  void Detach() {
    if (player_) {
      player_->RemoveClient(this);
      player_ = nullptr;
    }
  }

  RefPtr<JsTrack> GetTrack(std::shared_ptr<PubTrack> pub_track) {
    std::unique_lock<Mutex> lock(mutex_);
    for (auto& pair : tracks_) {
      if (pair.first == pub_track)
        return pair.second;
    }
    return nullptr;
  }

  void AddTrack(std::shared_ptr<PubTrack> pub_track) {
    std::unique_lock<Mutex> lock(mutex_);
    tracks_.emplace_back(pub_track, new JsTrack(pub_track));
    ScheduleEvent<events::Event>(EventType::AddTrack);
  }

  void RemoveTrack(std::shared_ptr<PubTrack> pub_track) {
    std::unique_lock<Mutex> lock(mutex_);
    for (auto it = tracks_.begin(); it != tracks_.end(); it++) {
      if (it->first == pub_track) {
        tracks_.erase(it);
        ScheduleEvent<events::Event>(EventType::RemoveTrack);
        return;
      }
    }
  }

 private:
  mutable Mutex mutex_;
  std::vector<std::pair<std::shared_ptr<PubTrack>, Member<JsTrack>>> tracks_;
  media::MediaPlayer* player_;
};

class AudioTrackList : public TrackList<AudioTrack, media::MediaTrack> {
  DECLARE_TYPE_INFO(AudioTrackList);

 public:
  using TrackList::TrackList;

 private:
  void OnAddAudioTrack(std::shared_ptr<media::MediaTrack> track) override;
  void OnRemoveAudioTrack(std::shared_ptr<media::MediaTrack> track) override;
};

class VideoTrackList : public TrackList<VideoTrack, media::MediaTrack> {
  DECLARE_TYPE_INFO(VideoTrackList);

 public:
  using TrackList::TrackList;

 private:
  void OnAddVideoTrack(std::shared_ptr<media::MediaTrack> track) override;
  void OnRemoveVideoTrack(std::shared_ptr<media::MediaTrack> track) override;
};

class TextTrackList : public TrackList<TextTrack, media::TextTrack> {
  DECLARE_TYPE_INFO(TextTrackList);

 public:
  using TrackList::TrackList;

 private:
  void OnAddTextTrack(std::shared_ptr<media::TextTrack> track) override;
  void OnRemoveTextTrack(std::shared_ptr<media::TextTrack> track) override;
};


template <typename T>
class TrackListFactory : public BackingObjectFactory<T, events::EventTarget> {
 public:
  TrackListFactory() {
    this->AddGenericProperty("length", &T::length);
    this->AddIndexer(&T::GetIndex);
  }
};

using AudioTrackListFactory = TrackListFactory<AudioTrackList>;
using VideoTrackListFactory = TrackListFactory<VideoTrackList>;
using TextTrackListFactory = TrackListFactory<TextTrackList>;

}  // namespace mse
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_MSE_TRACK_LIST_H_
