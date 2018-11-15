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

#include "src/test/media_files.h"

#include <glog/logging.h>

#include "src/util/file_system.h"

namespace shaka {

// Unlike in POSIX, we don't need to search for the media directory.
void InitMediaFiles(const char* arg0) {}

std::vector<uint8_t> GetMediaFile(const std::string& file_name) {
  const std::string path =
      util::FileSystem::GetPathForStaticFile(".", true, file_name);
  util::FileSystem fs;
  std::vector<uint8_t> ret;
  CHECK(fs.ReadFile(path, &ret));
  return ret;
}

}  // namespace shaka
