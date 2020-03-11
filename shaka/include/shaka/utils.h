// Copyright 2017 Google LLC
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

#ifndef SHAKA_EMBEDDED_UTILS_H_
#define SHAKA_EMBEDDED_UTILS_H_

#include <string>

#include "macros.h"
#include "media/media_player.h"

namespace shaka {

/**
 * @defgroup utils Utilities
 * @ingroup exported
 * A number of utility methods that an app may want to use.
 * @{
 */


/**
 * A simple struct representing a rectangle, with integer precision.  Units are
 * in pixels.
 */
struct SHAKA_EXPORT ShakaRect {
  int x;
  int y;
  int w;
  int h;

  bool operator==(const ShakaRect& other) const {
    return x == other.x && y == other.y && w == other.w && h == other.h;
  }
  bool operator!=(const ShakaRect& other) const {
    return !(*this == other);
  }
};


/**
 * Creates two rectangles that can be used as rendering destination to draw a
 * video with the given fill mode.  If the video exceeds the bounds specified,
 * this will use the @a src attribute to specify the region of the frame to
 * draw; otherwise that stores the full bounds of the frame.  The @a dest field
 * will contain the bounds to draw on the window.
 *
 * @param frame The bounds of the video.
 * @param bounds The bounds to draw the frame within.
 * @param mode The fill mode to fit to.
 * @param src Where to put the source rectangle.  This represents the sub-region
 *   of the frame to draw.
 * @param dest Where to put the destination rectangle.  This represents the
 *   region on the window to draw to.
 */
SHAKA_EXPORT void FitVideoToRegion(ShakaRect frame, ShakaRect bounds,
                                   media::VideoFillMode mode, ShakaRect* src,
                                   ShakaRect* dest);


/**
 * Creates a rectangle that can be used as a rendering destination to draw the
 * video while maintaining aspect ratio.  This will produce a rectangle that
 * will fit inside the window area, but will maintain the aspect ratio of the
 * video.
 *
 * @param video_width The width of the video.
 * @param video_height The height of the video.
 * @param window_width The width of the window, or the portion to use.
 * @param window_height The height of the window, or the portion to use.
 * @param window_x The X offset of the window region to use.
 * @param window_y The Y offset of the window region to use.
 */
SHAKA_EXPORT ShakaRect FitVideoToWindow(int video_width, int video_height,
                                        int window_width, int window_height,
                                        int window_x = 0, int window_y = 0);


/**
 * Escapes the given key-system name so it can appear in a config name path.
 * @param key_system The key system name (e.g. <code>com.widevine.alpha</code>).
 * @return A name path usable as part of Player::Configure.
 */
inline std::string EscapeKeySystem(const std::string& key_system) {
  std::string ret = key_system;
  std::string::size_type pos = 0;
  while ((pos = ret.find('.', pos)) != std::string::npos) {
    ret.insert(pos, "\\");
    pos += 2;
  }
  return ret;
}

/**
 * This creates a configuration key that sets the license server URL for the
 * given key system.
 *
 * \code{.cpp}
 * player.Configure(LicenseServerConfig("com.widevine.alpha"),
 *                  "https://example.com/server");
 * \endcode
 */
inline std::string LicenseServerConfig(const std::string& key_system) {
  return "drm.servers." + EscapeKeySystem(key_system);
}

/**
 * This creates a configuration key for advanced DRM configuration.
 *
 * \code{.cpp}
 * player.Configure(AdvancedDrmConfig("com.widevine.alpha", "videoRobustness"),
 *                  "SW_SECURE_DECODE");
 * \endcode
 */
inline std::string AdvancedDrmConfig(const std::string& key_system,
                                     const std::string& property) {
  return "drm.advanced." + EscapeKeySystem(key_system) + "." + property;
}

/** @} */

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTILS_H_
