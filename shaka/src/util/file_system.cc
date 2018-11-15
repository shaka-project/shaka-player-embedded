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

#ifdef OS_IOS
#  include <CoreFoundation/CoreFoundation.h>
#endif

#include <glog/logging.h>

#include <fstream>

#include "src/util/cfref.h"

namespace shaka {
namespace util {

namespace {

#ifdef OS_IOS
std::string GetBundleDir() {
  CFRef<CFURLRef> url(CFBundleCopyBundleURL(CFBundleGetMainBundle()));
  CFRef<CFStringRef> str(CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle));
  return CFStringGetCStringPtr(str, CFStringGetSystemEncoding());
}
#endif

}  // namespace

FileSystem::FileSystem() {}
FileSystem::~FileSystem() {}

// static
std::string FileSystem::GetPathForStaticFile(const std::string& static_data_dir,
                                             bool is_bundle_relative,  // NOLINT
                                             const std::string& file) {
#ifdef OS_IOS
  if (is_bundle_relative)
    return PathJoin(PathJoin(GetBundleDir(), static_data_dir), file);
#endif
  return PathJoin(static_data_dir, file);
}

// static
std::string FileSystem::GetPathForDynamicFile(
    const std::string& dynamic_data_dir, const std::string& file) {
  return PathJoin(dynamic_data_dir, file);
}

ssize_t FileSystem::FileSize(const std::string& path) const {
  std::ifstream file(path, std::ios::binary | std::ios::in);
  if (!file) {
    PLOG(ERROR) << "Error opening file '" << path << "'";
    return -1;
  }

  // Get the total file size.
  file.seekg(0, std::ios::end);
  if (file.fail()) {
    PLOG(ERROR) << "Error seeking in file";
    return -1;
  }

  return file.tellg();
}

bool FileSystem::ReadFile(const std::string& path,
                          std::vector<uint8_t>* data) const {
  std::ifstream file(path, std::ios::binary | std::ios::in);
  if (!file) {
    PLOG(ERROR) << "Error opening file '" << path << "'";
    return false;
  }

  // Get the total file size.
  file.seekg(0, std::ios::end);
  CHECK(!file.fail());
  const std::streamsize file_size = file.tellg();
  file.seekg(0);
  DCHECK(file);

  data->resize(file_size);
  file.read(reinterpret_cast<char*>(data->data()), file_size);
  if (!file) {
    PLOG(ERROR) << "Error reading file '" << path << "'";
    return false;
  }
  DCHECK_EQ(file_size, file.gcount());

  return true;
}

bool FileSystem::WriteFile(const std::string& path,
                           const std::vector<uint8_t>& data) const {
  std::ofstream file(path, std::ios::binary | std::ios::out | std::ios::trunc);
  if (!file) {
    PLOG(ERROR) << "Error opening file '" << path << "'";
    return false;
  }

  // Write will set file.bad() if it doesn't write all the characters.
  file.write(reinterpret_cast<const char*>(data.data()), data.size());
  if (!file) {
    PLOG(ERROR) << "Error writing file '" << path << "'";
    return false;
  }
  DCHECK_EQ(static_cast<std::streamsize>(data.size()), file.tellp());

  return true;
}

}  // namespace util
}  // namespace shaka
