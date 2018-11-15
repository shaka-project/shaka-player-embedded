// Copyright 2016 Google LLC
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

#include "src/util/utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace shaka {
namespace util {

using testing::ElementsAre;

TEST(UtilsTest, TrimAsciiWhitespace) {
  EXPECT_EQ("", TrimAsciiWhitespace(""));
  EXPECT_EQ("", TrimAsciiWhitespace("   \r\n"));
  EXPECT_EQ("abc", TrimAsciiWhitespace("abc"));
  EXPECT_EQ("abc", TrimAsciiWhitespace("  abc"));
  EXPECT_EQ("abc", TrimAsciiWhitespace("  \r\n  \nabc"));
  EXPECT_EQ("abc", TrimAsciiWhitespace("abc  \r\n"));
  EXPECT_EQ("abc", TrimAsciiWhitespace("  abc  \n"));
  EXPECT_EQ("a  b  \n c", TrimAsciiWhitespace("a  b  \n c"));
}

TEST(UtilsTest, StringSplit) {
  EXPECT_THAT(StringSplit("foo", '.'), ElementsAre("foo"));
  EXPECT_THAT(StringSplit("foo.bar", '.'), ElementsAre("foo", "bar"));
  EXPECT_THAT(StringSplit("foo.bar.baz", '.'),
              ElementsAre("foo", "bar", "baz"));
  EXPECT_THAT(StringSplit("foo.bar.baz", ':'), ElementsAre("foo.bar.baz"));
  EXPECT_THAT(StringSplit("foo.bar:baz", ':'), ElementsAre("foo.bar", "baz"));
  EXPECT_THAT(StringSplit("foo..bar", '.'), ElementsAre("foo", "", "bar"));
  EXPECT_THAT(StringSplit(".foo.bar", '.'), ElementsAre("", "foo", "bar"));
  EXPECT_THAT(StringSplit("foo.bar.", '.'), ElementsAre("foo", "bar", ""));
  EXPECT_THAT(StringSplit(".", '.'), ElementsAre("", ""));
  EXPECT_THAT(StringSplit("", '.'), ElementsAre(""));
}

TEST(UtilsTest, StringPrintf) {
  EXPECT_EQ("foo", StringPrintf("foo"));
  EXPECT_EQ("foo bar", StringPrintf("foo %s", "bar"));
  EXPECT_EQ("foo 0x000003", StringPrintf("foo 0x%06d", 3));
  EXPECT_EQ("foo 1 2 3\n", StringPrintf("foo %d %d %d\n", 1, 2, 3));

  const char buffer[] = "foo\0bar\nbaz";
  const std::string expected(buffer, buffer + sizeof(buffer) - 1);
  EXPECT_EQ(expected, StringPrintf("foo%cbar\nbaz", '\0'));
}

}  // namespace util
}  // namespace shaka
