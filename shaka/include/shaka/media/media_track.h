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

#ifndef SHAKA_EMBEDDED_MEDIA_MEDIA_TRACK_H_
#define SHAKA_EMBEDDED_MEDIA_MEDIA_TRACK_H_

#include <memory>
#include <string>

#include "../macros.h"

namespace shaka {
namespace media {

/**
 * Represents the type of the audio/video track.
 * @see https://html.spec.whatwg.org/multipage/media.html#dom-audiotrack-kind
 * @ingroup media
 */
enum class MediaTrackKind : uint8_t {
  Unknown,

  /**
   * A possible alternative to the main track, e.g. a different take of a song
   * (audio), or a different angle (video).
   */
  Alternative,

  /**
   * A version of the main video track with captions burnt in. (For legacy
   * content; new content would use text tracks.)
   */
  Captions,

  /** An audio description of a video track. */
  Descriptions,

  /** The primary audio or video track. */
  Main,

  /** The primary audio track, mixed with audio descriptions. */
  MainDesc,

  /** A sign-language interpretation of an audio track. */
  Sign,

  /**
   * A version of the main video track with subtitles burnt in. (For legacy
   * content; new content would use text tracks.)
   */
  Subtitles,

  /** A translated version of the main audio track. */
  Translation,

  /**
   * Commentary on the primary audio or video track, e.g. a director's
   * commentary.
   */
  Commentary,
};

/**
 * This defines a audio/video track.
 *
 * @ingroup media
 */
class SHAKA_EXPORT MediaTrack {
 public:
  MediaTrack(MediaTrackKind kind, const std::string& label,
            const std::string& language, const std::string& id);
  MediaTrack(MediaTrack&&) = delete;
  MediaTrack(const MediaTrack&) = delete;
  virtual ~MediaTrack();

  MediaTrack& operator=(const MediaTrack&) = delete;
  MediaTrack& operator=(MediaTrack&&) = delete;

  /** The label string of the track. */
  const std::string label;

  /** The language string of the track. */
  const std::string language;

  /** The id string of the track. */
  const std::string id;

  /** The kind of the track. */
  const MediaTrackKind kind;


  /** @return Whether the track is currently being played. */
  virtual bool enabled() const;

  /** Changes whether this track is currently being played. */
  virtual void SetEnabled(bool enabled);

 private:
  bool enabled_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_MEDIA_TRACK_H_
