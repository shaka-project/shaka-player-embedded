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

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "shaka/media/text_track.h"
#include "src/debug/mutex.h"
#include "src/util/utils.h"

namespace shaka {
namespace media {

class TextTrack::Impl {
 public:
  Impl() : mutex("TextTrack"), mode(TextTrackMode::Disabled) {}

  Mutex mutex;
  TextTrackMode mode;
  std::vector<std::shared_ptr<VTTCue>> cues;
  std::unordered_set<Client*> clients;
};

TextTrack::Client::Client() {}
TextTrack::Client::~Client() {}

TextTrack::TextTrack(TextTrackKind kind, const std::string& label,
                     const std::string& language, const std::string& id)
    : kind(kind), label(label), language(language), id(id), impl_(new Impl) {}
TextTrack::~TextTrack() {}

TextTrackMode TextTrack::mode() const {
  std::unique_lock<Mutex> lock(impl_->mutex);
  return impl_->mode;
}

void TextTrack::SetMode(TextTrackMode mode) {
  std::unique_lock<Mutex> lock(impl_->mutex);
  impl_->mode = mode;
}


std::vector<std::shared_ptr<VTTCue>> TextTrack::cues() const {
  std::unique_lock<Mutex> lock(impl_->mutex);
  return impl_->cues;
}

std::vector<std::shared_ptr<VTTCue>> TextTrack::active_cues(double time) const {
  auto cues = this->cues();

  std::vector<std::shared_ptr<VTTCue>> ret;
  for (auto& cue : cues) {
    if (cue->start_time() <= time && cue->end_time() >= time)
      ret.emplace_back(cue);
  }
  return ret;
}

double TextTrack::NextCueChangeTime(double time) const {
  auto cues = this->cues();

  double next_time = INFINITY;
  for (auto& cue : cues) {
    if (cue->start_time() > time)
      next_time = std::min(next_time, cue->start_time());
    else if (cue->end_time() > time)
      next_time = std::min(next_time, cue->end_time());
  }
  return next_time;
}

void TextTrack::AddCue(std::shared_ptr<VTTCue> cue) {
  std::unique_lock<Mutex> lock(impl_->mutex);
  impl_->cues.emplace_back(cue);

  for (Client* client : impl_->clients)
    client->OnCueAdded(cue);
}

void TextTrack::RemoveCue(std::shared_ptr<VTTCue> cue) {
  std::unique_lock<Mutex> lock(impl_->mutex);
  util::RemoveElement(&impl_->cues, cue);

  for (Client* client : impl_->clients)
    client->OnCueRemoved(cue);
}


void TextTrack::AddClient(Client* client) {
  std::unique_lock<Mutex> lock(impl_->mutex);
  impl_->clients.insert(client);
}

void TextTrack::RemoveClient(Client* client) {
  std::unique_lock<Mutex> lock(impl_->mutex);
  impl_->clients.erase(client);
}

}  // namespace media
}  // namespace shaka
