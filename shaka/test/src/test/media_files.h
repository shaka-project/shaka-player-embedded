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

#ifndef SHAKA_EMBEDDED_TEST_TEST_MEDIA_FILES_H_
#define SHAKA_EMBEDDED_TEST_TEST_MEDIA_FILES_H_

#include <string>
#include <vector>

namespace shaka {

/**
 * Searches for the media files directory.  This needs to be called during
 * initialization and should be given argv[0].
 */
void InitMediaFiles(const char* arg0);

/** Gets the bytes from the media file with the given name. */
std::vector<uint8_t> GetMediaFile(const std::string& file_name);

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_TEST_TEST_MEDIA_FILES_H_
