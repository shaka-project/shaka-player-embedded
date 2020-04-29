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

#include <iostream>
#include <string>
#include <type_traits>

#include "macros.h"

namespace shaka {

/**
 * @defgroup utils Utilities
 * @ingroup exported
 * A number of utility methods that an app may want to use.
 * @{
 */

/**
 * Defines possible fill modes for the video.  When drawing the video onto a
 * region, this determines how the video gets resized to fit.  The video frame
 * will always be centered within the region.
 */
enum class VideoFillMode : uint8_t {
  /**
   * Maintain the aspect ratio of the original video and size the video based on
   * the smaller of the extents.  There will be black bars around the video if
   * the region's aspect ratio isn't the same as the video's
   */
  MaintainRatio,

  /**
   * Stretch the video to completely fill the region.
   */
  Stretch,

  /**
   * Maintain the aspect ratio of the original video and size the video based on
   * the larger of the extents.  This will cause the video to be cropped to fit
   * in the region, but there won't be any black bars around the video.
   */
  Zoom,
};


/**
 * A simple struct representing a rectangle, with integer precision.  Units are
 * in pixels.
 */
template <typename T>
struct SHAKA_EXPORT ShakaRect {
  T x;
  T y;
  T w;
  T h;

  bool operator==(const ShakaRect& other) const {
    return x == other.x && y == other.y && w == other.w && h == other.h;
  }
  bool operator!=(const ShakaRect& other) const {
    return !(*this == other);
  }
};

/**
 * Defines a rational number (i.e. a fraction) in a way to reduce the number
 * of rounding errors.
 */
template <typename T>
struct SHAKA_EXPORT Rational final {
 private:
  template <typename U>
  using common = typename std::common_type<T, U>::type;
  template <typename U>
  using enable_if_num =
      typename std::enable_if<std::is_arithmetic<U>::value>::type;

 public:
  Rational() = default;
  Rational(T num, T den) : numerator(num), denominator(den) {}

  T truncate() const {
    return numerator / denominator;
  }
  Rational<T> inverse() const {
    return {denominator, numerator};
  }
  Rational<T> reduce() const {
    T a = numerator;
    T b = denominator;
    if (a == 0 || b == 0)
      return {0, 0};

    // Calculate the gcd(a, b) using the euclidean algorithm.
    while (b != 0) {
      T temp = a % b;
      a = b;
      b = temp;
    }
    return {numerator / a, denominator / a};
  }

  operator bool() const {
    return numerator != 0 && denominator != 0;
  }
  operator double() const {
    return static_cast<double>(numerator) / denominator;
  }

  template <typename U>
  bool operator==(const Rational<U>& other) const {
    const Rational<T> a = reduce();
    const Rational<U> b = other.reduce();
    return a.numerator == b.numerator && a.denominator == b.denominator;
  }

  template <typename U>
  bool operator!=(const Rational<U>& other) const {
    return !(*this == other);
  }

  template <typename U>
  Rational<common<U>> operator*(const Rational<U>& other) const {
    return {numerator * other.numerator, denominator * other.denominator};
  }

  template <typename U, typename = enable_if_num<U>>
  Rational<common<U>> operator*(U other) const {
    return {numerator * other, denominator};
  }

  template <typename U>
  Rational<common<U>> operator/(const Rational<U>& other) const {
    return {numerator * other.denominator, denominator * other.numerator};
  }

  template <typename U, typename = enable_if_num<U>>
  Rational<common<U>> operator/(U other) const {
    return {numerator, denominator * other};
  }

  T numerator;
  T denominator;
};
static_assert(std::is_pod<Rational<int>>::value, "Rational should be POD");

template <typename T>
std::ostream& operator<<(std::ostream& os, const Rational<T>& bar) {
  return os << bar.numerator << "/" << bar.denominator;
}

// operator*(T, Rational<T>);
template <typename T, typename U, typename =
      typename std::enable_if<std::is_arithmetic<T>::value>::type>
Rational<typename std::common_type<T, U>::type> operator*(
    T a, const Rational<U>& b) {
  return {a * b.numerator, b.denominator};
}

// operator/(T, Rational<T>);
template <typename T, typename U, typename =
      typename std::enable_if<std::is_arithmetic<T>::value>::type>
Rational<typename std::common_type<T, U>::type> operator/(
    T a, const Rational<U>& b) {
  return b.inverse() * a;
}


/**
 * Creates two rectangles that can be used as rendering destination to draw a
 * video with the given fill mode.  If the video exceeds the bounds specified,
 * this will use the @a src attribute to specify the region of the frame to
 * draw; otherwise that stores the full bounds of the frame.  The @a dest field
 * will contain the bounds to draw on the window.
 *
 * @param frame The bounds of the video.
 * @param bounds The bounds to draw the frame within.
 * @param sample_aspect_ratio The sample aspect ratio; this is the aspect ratio
 *   of the pixels in the image.  (0, 0) is treated as (1, 1).
 * @param mode The fill mode to fit to.
 * @param src Where to put the source rectangle.  This represents the sub-region
 *   of the frame to draw.
 * @param dest Where to put the destination rectangle.  This represents the
 *   region on the window to draw to.
 */
SHAKA_EXPORT void FitVideoToRegion(ShakaRect<uint32_t> frame,
                                   ShakaRect<uint32_t> bounds,
                                   Rational<uint32_t> sample_aspect_ratio,
                                   VideoFillMode mode, ShakaRect<uint32_t>* src,
                                   ShakaRect<uint32_t>* dest);


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
