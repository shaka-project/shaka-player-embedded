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

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "src/test/media_files.h"
#include "src/util/file_system.h"
#include "src/util/macros.h"

namespace shaka {

BEGIN_ALLOW_COMPLEX_STATICS
DEFINE_string(media_directory, "",
              "The directory that holds the test media files.");
END_ALLOW_COMPLEX_STATICS

/** An existing media file to signal the media directory. */
constexpr const char* kSignalFile = "clear_low.mp4";

/**
 * The path to the media directory, relative to a build directory using
 * --config-name.
 */
constexpr const char* kRelativePath = "../../shaka/test/media";

void InitMediaFiles(const char* arg0) {
  util::FileSystem fs;
  if (FLAGS_media_directory.empty()) {
    // Look for it in kRelativePath relative to where the executable is.
    const std::string test_dir = util::FileSystem::PathJoin(
        util::FileSystem::DirName(arg0), kRelativePath);
    if (fs.FileExists(util::FileSystem::PathJoin(test_dir, kSignalFile))) {
      FLAGS_media_directory = test_dir;
    } else {
      LOG(FATAL) << "Unable to find the test media directory.  Pass "
                    "--media_directory to give an explicit path.";
    }
  } else if (!fs.FileExists(util::FileSystem::PathJoin(FLAGS_media_directory,
                                                       kSignalFile))) {
    LOG(FATAL) << "Invalid value for --media_directory.  It should point to "
                  "\"shaka/test/media\" in the source directory.";
  }
}

std::vector<uint8_t> GetMediaFile(const std::string& file_name) {
  const std::string path =
      util::FileSystem::PathJoin(FLAGS_media_directory, file_name);
  util::FileSystem fs;
  std::vector<uint8_t> ret;
  CHECK(fs.ReadFile(path, &ret));
  return ret;
}

}  // namespace shaka
