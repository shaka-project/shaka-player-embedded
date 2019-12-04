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

#ifndef SHAKA_EMBEDDED_TEXT_TRACK_H_
#define SHAKA_EMBEDDED_TEXT_TRACK_H_

#include <memory>
#include <vector>

#include "macros.h"
#include "vtt_cue.h"

namespace shaka {
namespace js {
namespace mse {

class TextTrack;

}  // namespace mse
}  // namespace js

/**
 * Represents the type of the text track.
 * @see https://html.spec.whatwg.org/multipage/media.html#text-track-kind
 * @ingroup player
 */
enum class TextTrackKind {
  /**
   * The track defines subtitles.
   */
  Subtitles,
  /**
   * The text track defines dialogue and sound effects, for the deaf.
   */
  Captions,
  /**
   * The text track defines a textual description of the video, for the blind.
   */
  Descriptions,
  /**
   * The text track defines chapter titles, for navigation.
   */
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
 * @ingroup player
 */
enum class TextTrackMode {
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
  /**
   * The text track is enabled and visible.
   */
  Showing,
};

/**
 * This defines a text track that stores text cues.
 * This is a wrapper over an internal type.
 *
 * @see https://html.spec.whatwg.org/multipage/media.html#the-track-element
 * @ingroup player
 */
class SHAKA_EXPORT TextTrack final {
 public:
  TextTrack(TextTrack&&);
  TextTrack(const TextTrack&) = delete;
  ~TextTrack();

  TextTrack& operator=(const TextTrack&) = delete;
  TextTrack& operator=(TextTrack&& other);

  /** @return The kind of the text track. */
  TextTrackKind kind();
  /** Sets the kind of the text track. */
  void SetKind(TextTrackKind kind);
  /** @return The label string of the text track. */
  std::string label();
  /** Sets the label string of the text track. */
  void SetLabel(const std::string label);
  /** @return The language string of the text track. */
  std::string language();
  /** Sets the language string of the text track. */
  void SetLanguage(const std::string language);
  /** @return The id string of the text track. */
  std::string id();
  /** Sets the id string of the text track. */
  void SetId(const std::string id);
  /** @return The mode of the text track. */
  TextTrackMode mode();
  /** Sets the mode of the text track. */
  void SetMode(TextTrackMode mode);

  /**
   * @return A vector containing the list of cues in the text track.
   *   Note that this is a copy of the internal cue list, so adding or removing
   *   elements directly from the vector will not change the internal cue list.
   *   Modifying the contents of individual cues will work, on the other hand.
   *   The cues will remain in memory until the video is unloaded.
   */
  std::vector<VTTCue*> cues();

  /** Adds a copy of the provided cue to the list of cues in the text track. */
  void AddCue(const VTTCue& cue);

  /**
   * Removes an element from the list of cues in the text track.
   * Note that the provided cue has to be a cue retrieved from the cues()
   * method; a shaka::VTTCue object will not be valid otherwise, even if it
   * has been added with AddCue.
   */
  void RemoveCue(VTTCue* cue);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;

  friend class Video;
  TextTrack(js::mse::TextTrack* inner);
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_TEXT_TRACK_H_
