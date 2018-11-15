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

#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glog/logging.h>

namespace shaka {
namespace util {

namespace {

const char kDirectorySeparator = '/';

}  // namespace

// static
std::string FileSystem::PathJoin(const std::string& a, const std::string& b) {
  if (b.empty())
    return a;
  if (a.empty() || b[0] == kDirectorySeparator)
    return b;
  if (a[a.size() - 1] == kDirectorySeparator)
    return a + b;
  return a + kDirectorySeparator + b;
}

// static
std::string FileSystem::DirName(const std::string& path) {
  // Create a copy of |path|.  Then dirname will edit the string to put a
  // null char at the last slash.  When it returns the argument, the string
  // constructor will copy the string up to that null char.
  std::string copy = path;
  return dirname(&copy[0]);
}

bool FileSystem::FileExists(const std::string& path) const {
  // Cannot use fstream for this since it allows opening directories.
  struct stat info;
  return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

bool FileSystem::DirectoryExists(const std::string& path) const {
  struct stat info;
  return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

bool FileSystem::DeleteFile(const std::string& path) const {
  return unlink(path.c_str()) == 0;
}

bool FileSystem::CreateDirectory(const std::string& path) const {
  std::string::size_type pos = 0;
  while ((pos = path.find(kDirectorySeparator, pos + 1)) != std::string::npos) {
    if (mkdir(path.substr(0, pos).c_str(), 0755) != 0 && errno != EEXIST)
      return false;
  }

  return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
}

bool FileSystem::ListFiles(const std::string& path,
                           std::vector<std::string>* files) const {
  files->clear();

  DIR* dir;
  dirent* entry;
  if ((dir = opendir(path.c_str())) == nullptr) {
    PLOG(ERROR) << "Unable to open directory '" << path << "'";
    return false;
  }

  while ((entry = readdir(dir)) != nullptr) {
    if (!strcmp(".", entry->d_name) || !strcmp("..", entry->d_name))
      continue;

    const std::string sub_path = PathJoin(path, entry->d_name);
    struct stat info;
    CHECK_EQ(0, stat(sub_path.c_str(), &info));
    if (S_ISREG(info.st_mode))
      files->push_back(entry->d_name);
    else if (!S_ISDIR(info.st_mode))
      LOG(WARNING) << "Unable to process folder entry '" << sub_path << "'";
  }
  closedir(dir);

  return true;
}

}  // namespace util
}  // namespace shaka
