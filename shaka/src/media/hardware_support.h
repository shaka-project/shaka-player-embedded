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

#ifndef SHAKA_EMBEDDED_MEDIA_HARDWARE_SUPPORT_H_
#define SHAKA_EMBEDDED_MEDIA_HARDWARE_SUPPORT_H_

#include <string>

namespace shaka {
namespace media {

#ifdef FORCE_HARDWARE_DECODE

/**
 * Determines if the current hardware supports the given codec and video size.
 * This will fail to compile on platforms that don't have a specific hardware
 * decoder.
 *
 * @param codec The input codec (e.g. "avc1.4d401f").
 * @param width The width of the video, in pixels.
 * @param height The height of the video, in pixels.
 * @return Whether the hardware supports the given parameters.
 */
bool DoesHardwareSupportCodec(const std::string& codec, int width, int height);

#endif

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_HARDWARE_SUPPORT_H_
