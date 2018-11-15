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

#include "src/util/file_system.h"

#include <Shlwapi.h>

#include <glog/logging.h>

namespace shaka {
namespace util {

// static
std::string FileSystem::PathJoin(const std::string& a, const std::string& b) {
  std::string ret(MAX_PATH, '\0');
  if (!PathCombineA(ret.c_str(), a.c_str(), b.c_str()))
    return "";
  ret.resize(strlen(ret.c_str()));
  return ret;
}

// static
std::string FileSystem::DirName(const std::string& path) {
#error "Not implemented for Windows"
}

bool FileSystem::FileExists(const std::string& path) const {
  // Cannot use fstream for this since it allows opening directories.
  return PathFileExists(path.c_str());
}

bool FileSystem::DirectoryExists(const std::string& path) const {
  return PathIsDirectory(path.c_str());
}

bool FileSystem::ListFiles(const std::string& path,
                           std::vector<std::string>* files) const {
  files->clear();

#error "Not implemented for Windows"

  return true;
}

}  // namespace util
}  // namespace shaka
