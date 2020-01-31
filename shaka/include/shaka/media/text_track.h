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

#ifndef SHAKA_EMBEDDED_MEDIA_TEXT_TRACK_H_
#define SHAKA_EMBEDDED_MEDIA_TEXT_TRACK_H_

#include <memory>
#include <string>
#include <vector>

#include "../macros.h"
#include "vtt_cue.h"

namespace shaka {
namespace media {

/**
 * Represents the type of the text track.
 * @see https://html.spec.whatwg.org/multipage/media.html#text-track-kind
 * @ingroup media
 */
enum class TextTrackKind : uint8_t {
  /** The track defines subtitles. */
  Subtitles,

  /** The text track defines dialogue and sound effects, for the deaf. */
  Captions,

  /**
   * The text track defines a textual description of the video, for the blind.
   */
  Descriptions,

  /** The text track defines chapter titles, for navigation. */
  Chapters,

  /**
   * The text track defines content for use by scripts, which will not be viewed
   * by users.
   */
  Metadata,
};

/**
 * Represents the current state of the text track.
 * @see https://html.spec.whatwg.org/multipage/media.html#text-track-mode
 * @ingroup media
 */
enum class TextTrackMode : uint8_t {
  /**
   * The text track is currently disabled. The user agent is completely ignoring
   * it.
   */
  Disabled,

  /**
   * The text track is active, but the cues are not being displayed. Events will
   * still fire as appropriate.
   */
  Hidden,

  /** The text track is enabled and visible. */
  Showing,
};

/**
 * This defines a text track that stores text cues.  This type can be subclassed
 * to provide additional behavior, or can be used as-is as a list of cue
 * objects.
 *
 * This type is internally thread-safe.
 *
 * @ingroup media
 */
class SHAKA_EXPORT TextTrack {
 public:
  /**
   * An interface for events that happen on the track.  These can be called from
   * any thread.  These are called with a lock held, so the callbacks cannot
   * call back into this object.
   */
  class Client {
   public:
    SHAKA_DECLARE_INTERFACE_METHODS(Client);

    /** Called when a cue is added to the track. */
    virtual void OnCueAdded(std::shared_ptr<VTTCue> cue) = 0;

    /** Called when a cue is removed from the track. */
    virtual void OnCueRemoved(std::shared_ptr<VTTCue> cue) = 0;
  };

  TextTrack(TextTrackKind kind, const std::string& label,
            const std::string& language, const std::string& id);
  virtual ~TextTrack();

  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(TextTrack);

  /** The kind of the text track. */
  const TextTrackKind kind;

  /** The label string of the text track. */
  const std::string label;

  /** The language string of the text track. */
  const std::string language;

  /** The id string of the text track. */
  const std::string id;


  /** @return The mode of the text track. */
  virtual TextTrackMode mode() const;

  /** Sets the mode of the text track. */
  virtual void SetMode(TextTrackMode mode);


  /** @return The list of cues in the text track. */
  virtual std::vector<std::shared_ptr<VTTCue>> cues() const;

  /** @return The list of cues that should be displayed at the given time. */
  virtual std::vector<std::shared_ptr<VTTCue>> active_cues(double time) const;

  /**
   * Gets the time that the active cue list should change based on the current
   * list of cues.  This will be the nearest start or end time after the given
   * @a time.  This allows the app to delay polling until something is expected
   * to change.  This will return Infinity if there is nothing after @a time.
   *
   * @param time The media time to check.
   * @return The next media time an active cue will change.
   */
  double NextCueChangeTime(double time) const;

  /** Adds the provided cue to the list of cues in the text track. */
  virtual void AddCue(std::shared_ptr<VTTCue> cue);

  /** Removes the given cue from the list of cues. */
  virtual void RemoveCue(std::shared_ptr<VTTCue> cue);


  /** Adds the given client to receive calls for events. */
  void AddClient(Client* client);

  /** Removes the given client from receiving calls for events. */
  void RemoveClient(Client* client);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_TEXT_TRACK_H_
