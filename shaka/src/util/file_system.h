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

#ifndef SHAKA_EMBEDDED_UTIL_FILE_SYSTEM_H_
#define SHAKA_EMBEDDED_UTIL_FILE_SYSTEM_H_

#include <string>
#include <vector>

#include "src/util/macros.h"

namespace shaka {
namespace util {

/**
 * An abstraction of the file system.  This manages interactions with the file
 * system like reading and writing files.
 */
class FileSystem {
 public:
  FileSystem();
  virtual ~FileSystem();


  /** @return A path that is the result of combining @a a and @a b. */
  static std::string PathJoin(const std::string& a, const std::string& b);

  /** @return The directory name of the given path. */
  static std::string DirName(const std::string& path);

  /** @return The full path to the given static file. */
  static std::string GetPathForStaticFile(const std::string& static_data_dir,
                                          bool is_bundle_relative,
                                          const std::string& file);

  /** @return The full path to the given dynamic file. */
  static std::string GetPathForDynamicFile(const std::string& dynamic_data_dir,
                                           const std::string& file);


  /** @return Whether the given file exists (must be a file). */
  virtual bool FileExists(const std::string& path) const;

  /** @return Whether the given directory exists (must be a directory). */
  virtual bool DirectoryExists(const std::string& path) const;

  /** @return The size of the given file, or -1 on error. */
  virtual ssize_t FileSize(const std::string& path) const;

  /**
   * Deletes the given file, file must already exist.
   * @param path The path to the file to delete.
   * @return True on success, false on error.
   */
  MUST_USE_RESULT virtual bool DeleteFile(const std::string& path) const;

  /**
   * Creates a directory (and any parent directories) at the given path.
   * @param path The path to the directory to create.
   * @return True on success, false on error.
   */
  MUST_USE_RESULT virtual bool CreateDirectory(const std::string& path) const;

  /**
   * Lists the files that are in the given directory.
   * @param path The path to the directory to search.
   * @param files [OUT] Will contain the names of the files contained.
   * @return True on success, false on error.
   */
  MUST_USE_RESULT virtual bool ListFiles(const std::string& path,
                                         std::vector<std::string>* files) const;


  /**
   * @param path The path of the file to read.
   * @param data [OUT] Where to put the contents of the file.
   * @return True on success, false on error.
   */
  MUST_USE_RESULT virtual bool ReadFile(const std::string& path,
                                        std::vector<uint8_t>* data) const;

  /**
   * @param path The path of the file to write to.
   * @param data The data to write into the file.
   * @return True on success, false on error.
   */
  MUST_USE_RESULT virtual bool WriteFile(
      const std::string& path, const std::vector<uint8_t>& data) const;
};

}  // namespace util
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTIL_FILE_SYSTEM_H_
