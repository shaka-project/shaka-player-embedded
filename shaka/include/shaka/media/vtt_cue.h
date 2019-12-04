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

#ifndef SHAKA_EMBEDDED_MEDIA_VTT_CUE_H_
#define SHAKA_EMBEDDED_MEDIA_VTT_CUE_H_

#include <mutex>
#include <string>

#include "../macros.h"

namespace shaka {
namespace media {

/**
 * Represents the direction to write the text.
 * @see https://w3c.github.io/webvtt/#webvtt-cue-writing-direction
 * @ingroup media
 */
enum class DirectionSetting : uint8_t {
  /**
   * A line extends horizontally and is offset vertically from the video
   * viewport’s top edge, with consecutive lines displayed below each other.
   */
  Horizontal,

  /**
   * A line extends vertically and is offset horizontally from the video
   * viewport’s left edge, with consecutive lines displayed to the right of each
   * other.
   */
  LeftToRight,

  /**
   * A line extends vertically and is offset horizontally from the video
   * viewport’s right edge, with consecutive lines displayed to the left of each
   * other.
   */
  RightToLeft,
};

/**
 * Represents the alignment of the cue box.
 * @see https://w3c.github.io/webvtt/#webvtt-cue-line-alignment
 * @ingroup media
 */
enum class LineAlignSetting : uint8_t {
  /**
   * The cue box’s top side (for horizontal cues), left side (for vertical
   * growing right), or right side (for vertical growing left) is aligned at the
   * line.
   */
  Start,

  /** The cue box is centered at the line. */
  Center,

  /**
   * The cue box’s bottom side (for horizontal cues), right side (for vertical
   * growing right), or left side (for vertical growing left) is aligned at the
   * line.
   */
  End,
};

/**
 * Represents where the position anchors the cue box.
 * @see https://w3c.github.io/webvtt/#webvtt-cue-position-alignment
 * @ingroup media
 */
enum class PositionAlignSetting : uint8_t {
  /**
   * The cue box’s left side (for horizontal cues) or top side (otherwise) is
   * aligned at the position.
   */
  LineLeft,

  /** The cue box is centered at the position. */
  Center,

  /**
   * The cue box’s right side (for horizontal cues) or bottom side (otherwise)
   * is aligned at the position.
   */
  LineRight,

  /**
   * The cue box’s alignment depends on the value of the text alignment of the
   * cue.
   */
  Auto,
};

/**
 * Represents the alignment of text within the cue box.
 * @see https://w3c.github.io/webvtt/#webvtt-cue-text-alignment
 * @ingroup media
 */
enum class AlignSetting : uint8_t {
  /**
   * The text of each line is individually aligned towards the start side of the
   * box.
   */
  Start,

  /** The text is aligned centered between the box’s start and end sides. */
  Center,

  /**
   * The text of each line is individually aligned towards the end side of the
   * box.
   */
  End,

  /**
   * The text is aligned to the box’s left side (for horizontal cues) or top
   * side (otherwise).
   */
  Left,

  /**
   * The text is aligned to the box’s right side (for horizontal cues) or bottom
   * side (otherwise).
   */
  Right,
};


/**
 * This defines a text cue that is used for subtitles or closed-captioning.
 * This type is internally thread-safe.
 *
 * @see https://w3c.github.io/webvtt/#the-vttcue-interface
 * @ingroup media
 */
class SHAKA_EXPORT VTTCue {
 public:
  VTTCue(double start_time, double end_time, const std::string& text);
  VTTCue(const VTTCue& cue);
  VTTCue(VTTCue&& cue);
  ~VTTCue();

  VTTCue& operator=(const VTTCue& cue);
  VTTCue& operator=(VTTCue&& cue);

  /** @name TextTrackCue */
  //@{
  /** @return The ID of the cue. */
  std::string id() const;
  /** Sets the ID of the cue to the given value. */
  void SetId(const std::string& id);
  /** @return The start time the Cue should be rendered at. */
  double start_time() const;
  /** Sets the start time the Cue should be rendered at. */
  void SetStartTime(double time);
  /** @return The end time the Cue should be rendered at. */
  double end_time() const;
  /** Sets the end time the Cue should be rendered at. */
  void SetEndTime(double time);
  /** @return Whether the media should pause when the cue stops rendering. */
  bool pause_on_exit() const;
  /** Sets whether the media should pause when the cue stops rendering. */
  void SetPauseOnExit(bool pause);
  //@}

  /** @name VTTCue */
  //@{
  /** @return The Cue's vertical direction setting. */
  DirectionSetting vertical() const;
  /** Sets the Cue's vertical direction setting. */
  void SetVertical(DirectionSetting setting);
  /** @return Whether the Cue snaps to lines. */
  bool snap_to_lines() const;
  /** Sets whether the Cue snaps to lines. */
  void SetSnapToLines(bool snap);
  /** @return The Cue's line align setting. */
  LineAlignSetting line_align() const;
  /** Sets the Cue's line align setting. */
  void SetLineAlign(LineAlignSetting align);
  /** @return The Cue's line value, or NAN if using 'auto'. */
  double line() const;
  /** Sets the Cue's line value, use NAN to signal 'auto'. */
  void SetLine(double line);
  /** @return The Cue's position value, or NAN if using 'auto'. */
  double position() const;
  /** Sets the Cue's position value, use NAN to signal 'auto'. */
  void SetPosition(double position);
  /** @return The Cue's position align setting. */
  PositionAlignSetting position_align() const;
  /** Sets the Cue's position align setting. */
  void SetPositionAlign(PositionAlignSetting align);
  /** @return The Cue's size. */
  double size() const;
  /** Sets the Cue's size. */
  void SetSize(double size);
  /** @return The align setting of the Cue. */
  AlignSetting align() const;
  /** Sets the align setting of the Cue. */
  void SetAlign(AlignSetting align);
  /** @return The text body of the Cue. */
  std::string text() const;
  /** Sets the text body of the cue. */
  void SetText(const std::string& text);
  //@}

 private:
  mutable std::mutex mutex_;

  std::string id_;
  std::string text_;
  double start_time_;
  double end_time_;
  double line_;
  double position_;
  double size_;
  DirectionSetting vertical_;
  LineAlignSetting line_align_;
  PositionAlignSetting position_align_;
  AlignSetting align_;
  bool snap_to_lines_;
  bool pause_on_exit_;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_VTT_CUE_H_
