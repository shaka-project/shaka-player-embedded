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

#include "src/util/dynamic_buffer.h"

#include <glog/logging.h>
#include <gtest/gtest.h>
#include <stdlib.h>

#include <vector>

namespace shaka {
namespace util {

namespace {

const char kData1[] = "First Data";
const char kData2[] = "Second\0Data";
// Number of characters in string (not counting trailing '\0').
const size_t kData1Size = sizeof(kData1) - 1;
const size_t kData2Size = sizeof(kData2) - 1;

std::vector<uint8_t> GetRandomBytes(size_t count) {
  std::vector<uint8_t> ret(count, 0);
  CHECK_EQ(ret.size(), count);
  for (size_t i = 0; i < count; i++)
    ret[i] = static_cast<uint8_t>(rand());
  return ret;
}

}  // namespace

class DynamicBufferTest : public testing::Test {
 protected:
  size_t GetMinBufferSize() const {
    return DynamicBuffer::kMinBufferSize;
  }
};

TEST_F(DynamicBufferTest, Size) {
  DynamicBuffer buf;
  buf.AppendCopy(kData1, kData1Size);
  buf.AppendCopy(kData2, kData2Size);

  EXPECT_EQ(kData1Size + kData2Size, buf.Size());

  buf.AppendCopy(kData1, kData1Size);
  EXPECT_EQ(kData1Size * 2 + kData2Size, buf.Size());
}

TEST_F(DynamicBufferTest, Clear) {
  DynamicBuffer buf;
  buf.AppendCopy(kData1, kData1Size);
  buf.AppendCopy(kData2, kData2Size);

  EXPECT_EQ(kData1Size + kData2Size, buf.Size());
  buf.Clear();
  EXPECT_EQ(0u, buf.Size());
  EXPECT_EQ("", buf.CreateString());
}

TEST_F(DynamicBufferTest, CreateString) {
  DynamicBuffer buf;
  buf.AppendCopy(kData1, kData1Size);
  buf.AppendCopy(kData2, kData2Size);

  std::string expected(kData1, kData1Size);
  expected.append(kData2, kData2Size);
  EXPECT_EQ(expected, buf.CreateString());
}

TEST_F(DynamicBufferTest, CopyDataTo) {
  DynamicBuffer buf;
  buf.AppendCopy(kData1, kData1Size);
  buf.AppendCopy(kData2, kData2Size);

  std::vector<uint8_t> expected(kData1, kData1 + kData1Size);
  expected.insert(expected.end(), kData2, kData2 + kData2Size);

  std::vector<uint8_t> actual;
  actual.resize(buf.Size());
  buf.CopyDataTo(&actual[0], actual.size());
  EXPECT_EQ(expected, actual);
}

TEST_F(DynamicBufferTest, OverflowBuffer) {
  DynamicBuffer buf;
  std::vector<uint8_t> temp = GetRandomBytes(GetMinBufferSize() - 100);
  std::vector<uint8_t> extra = GetRandomBytes(500);
  std::vector<uint8_t> expected = temp;
  expected.insert(expected.end(), extra.begin(), extra.end());

  buf.AppendCopy(temp.data(), temp.size());
  buf.AppendCopy(extra.data(), extra.size());

  std::vector<uint8_t> actual;
  EXPECT_EQ(GetMinBufferSize() + 400, buf.Size());
  actual.resize(buf.Size());
  buf.CopyDataTo(&actual[0], actual.size());
  EXPECT_EQ(expected, actual);
}

TEST_F(DynamicBufferTest, BiggerThanBuffer) {
  DynamicBuffer buf;
  std::vector<uint8_t> temp = GetRandomBytes(GetMinBufferSize() + 100);
  std::vector<uint8_t> extra = GetRandomBytes(500);
  std::vector<uint8_t> expected = temp;
  expected.insert(expected.end(), extra.begin(), extra.end());

  buf.AppendCopy(temp.data(), temp.size());
  buf.AppendCopy(extra.data(), extra.size());

  std::vector<uint8_t> actual;
  EXPECT_EQ(GetMinBufferSize() + 600, buf.Size());
  actual.resize(buf.Size());
  buf.CopyDataTo(&actual[0], actual.size());
  EXPECT_EQ(expected, actual);
}

}  // namespace util
}  // namespace shaka
