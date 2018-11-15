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

#ifdef OS_POSIX
#  include <ftw.h>
#endif
#include <glog/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdlib.h>

#include <fstream>

#include "src/util/darwin_utils.h"

namespace shaka {
namespace util {

using testing::UnorderedElementsAre;

class FileSystemTest : public testing::Test {
 public:
  void SetUp() override {
#ifdef OS_POSIX
#  ifdef OS_IOS
    temp_dir = GetTemporaryDirectory() + "/dirXXXXXX";
#  else
    temp_dir = "/tmp/dirXXXXXX";
#  endif
    if (!mkdtemp(&temp_dir[0]))
      PLOG(FATAL) << "Error creating temp directory";
    existing_file = temp_dir + "/" + "existing";
    non_exist = temp_dir + "/" + "non_existing";
    Touch(existing_file);
#else
#  error "Not implemented for Windows"
#endif
  }

  void TearDown() override {
#ifdef OS_POSIX
    // This traverses a directory tree and calls the given method.  FTW_DEPTH
    // makes this a post-order traversal instead of a pre-order traversal.
    if (nftw(temp_dir.c_str(), DeleteItem, FOPEN_MAX, FTW_DEPTH))
      PLOG(FATAL) << "Error traversing folder.";
#else
#  error "Not implemented for Windows"
#endif
  }

 protected:
  static void Touch(const std::string& path) {
    std::ofstream file(path, std::ios::out);
    if (!file)
      PLOG(FATAL) << "Unable to touch file " << path;
  }

#ifdef OS_POSIX
  static int DeleteItem(const char* path, const struct stat* st, int flags,
                        struct FTW*) {
    const int status = flags == FTW_DP ? rmdir(path) : unlink(path);
    if (status != 0) {
      PLOG(FATAL) << "Error deleting file/directory " << path << " with status "
                  << status;
    }
    return status;
  }
#endif

 protected:
  FileSystem fs;
  std::string temp_dir;
  std::string existing_file;
  std::string non_exist;
};

TEST_F(FileSystemTest, FileExists) {
  EXPECT_TRUE(fs.FileExists(existing_file));
  EXPECT_FALSE(fs.FileExists(temp_dir));
  EXPECT_FALSE(fs.FileExists(non_exist));
}

TEST_F(FileSystemTest, DirectoryExists) {
  EXPECT_TRUE(fs.DirectoryExists(temp_dir));
  EXPECT_FALSE(fs.DirectoryExists(existing_file));
  EXPECT_FALSE(fs.DirectoryExists(non_exist));
}

TEST_F(FileSystemTest, ListFiles) {
  // Note that SetUp/TearDown is called for EACH test, so this gets its own
  // directory.
  Touch(FileSystem::PathJoin(temp_dir, "other"));

  std::vector<std::string> files;
  ASSERT_TRUE(fs.ListFiles(temp_dir, &files));
  ASSERT_THAT(files, UnorderedElementsAre("existing", "other"));
}

TEST_F(FileSystemTest, ReadAndWrite) {
  const std::string path = FileSystem::PathJoin(temp_dir, "file");
  Touch(path);

  std::vector<uint8_t> file_data;
  ASSERT_TRUE(fs.ReadFile(path, &file_data));
  EXPECT_TRUE(file_data.empty());

  const std::vector<uint8_t> expected_data = {0x01, 0x02, 0x03, 0x04};
  ASSERT_TRUE(fs.WriteFile(path, expected_data));

  ASSERT_TRUE(fs.ReadFile(path, &file_data));
  EXPECT_EQ(expected_data, file_data);

  // Writing to an existing file should erase old data.
  ASSERT_TRUE(fs.WriteFile(path, expected_data));
  ASSERT_TRUE(fs.ReadFile(path, &file_data));
  EXPECT_EQ(expected_data, file_data);
}

TEST_F(FileSystemTest, FileSize) {
  const std::string path = FileSystem::PathJoin(temp_dir, "file");
  Touch(path);

  ASSERT_EQ(fs.FileSize(non_exist), -1);
  ASSERT_TRUE(fs.FileExists(path));
  ASSERT_EQ(fs.FileSize(path), 0);

  const std::vector<uint8_t> expected_data = {0x01, 0x02, 0x03, 0x04};
  ASSERT_TRUE(fs.WriteFile(path, expected_data));

  ASSERT_EQ(fs.FileSize(path), expected_data.size());
}

TEST_F(FileSystemTest, Delete) {
  const std::string path = FileSystem::PathJoin(temp_dir, "file");
  Touch(path);

  ASSERT_TRUE(fs.FileExists(path));
  ASSERT_TRUE(fs.DeleteFile(path));
  ASSERT_FALSE(fs.FileExists(path));
  ASSERT_FALSE(fs.DeleteFile(path));
}

TEST_F(FileSystemTest, CreateDirectory) {
  const std::string first_path = FileSystem::PathJoin(temp_dir, "dir");

  ASSERT_FALSE(fs.DirectoryExists(first_path));
  ASSERT_TRUE(fs.CreateDirectory(first_path));
  ASSERT_TRUE(fs.DirectoryExists(first_path));

  const std::string second_path = FileSystem::PathJoin(temp_dir, "dir2");
  const std::string nested_path = FileSystem::PathJoin(second_path, "nest");
  ASSERT_FALSE(fs.DirectoryExists(second_path));
  ASSERT_FALSE(fs.DirectoryExists(nested_path));
  ASSERT_TRUE(fs.CreateDirectory(nested_path));
  ASSERT_TRUE(fs.DirectoryExists(second_path));
  ASSERT_TRUE(fs.DirectoryExists(nested_path));
}

TEST_F(FileSystemTest, PathJoin) {
  // These tests can't be general because:
  // - There is a different path separator
  // - There is a different way to detect absolute paths
  // - Windows uses PathCombine, which will collapse .. paths, while POSIX won't
#ifdef OS_POSIX
  EXPECT_EQ("foo/bar/baz", FileSystem::PathJoin("foo/bar", "baz"));
  EXPECT_EQ("foo/bar/baz", FileSystem::PathJoin("foo", "bar/baz"));
  EXPECT_EQ("foo/..", FileSystem::PathJoin("foo", ".."));
  EXPECT_EQ("/usr/local/include",
            FileSystem::PathJoin("/usr/local", "include"));
  EXPECT_EQ("/usr", FileSystem::PathJoin("foo/bar", "/usr"));
  EXPECT_EQ("foo/bar", FileSystem::PathJoin("foo/bar", ""));
  EXPECT_EQ("foo/bar", FileSystem::PathJoin("", "foo/bar"));
#else
  EXPECT_EQ("foo\\bar", FileSystem::PathJoin("foo", "bar"));
  EXPECT_EQ("foo\\bar", FileSystem::PathJoin("foo\\baz", "..\\bar"));
  EXPECT_EQ("C:\\foo\\bar", FileSystem::PathJoin("C:\\foo", "bar"));
  EXPECT_EQ("C:\\foo", FileSystem::PathJoin("bar\\other", "C:\\foo"));
  EXPECT_EQ("foo\\bar", FileSystem::PathJoin("foo\\bar", ""));
  EXPECT_EQ("foo\\bar", FileSystem::PathJoin("", "foo\\bar"));
#endif
}

}  // namespace util
}  // namespace shaka
