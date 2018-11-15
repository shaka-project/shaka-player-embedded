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

#include "src/util/buffer_reader.h"

#include <gtest/gtest.h>

#include "src/util/macros.h"

namespace shaka {
namespace util {

TEST(BufferReaderTest, Read_BasicFlow) {
  const uint8_t buffer[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
  constexpr const size_t buffer_size = sizeof(buffer);
  BufferReader reader(buffer, buffer_size);
  ASSERT_FALSE(reader.empty());
  ASSERT_EQ(buffer_size, reader.BytesRemaining());

  uint8_t dest[buffer_size];
  ASSERT_EQ(buffer_size, reader.Read(dest, buffer_size));
  EXPECT_EQ(0u, reader.BytesRemaining());
  EXPECT_TRUE(reader.empty());
  EXPECT_EQ(0, memcmp(buffer, dest, buffer_size));
}

TEST(BufferReaderTest, Read_LessThanRemaining) {
  const uint8_t buffer[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
  constexpr const size_t buffer_size = sizeof(buffer);
  BufferReader reader(buffer, buffer_size);

  uint8_t dest[buffer_size];
  constexpr const size_t to_read = buffer_size - 4;
  ASSERT_EQ(to_read, reader.Read(dest, to_read));
  EXPECT_EQ(buffer_size - to_read, reader.BytesRemaining());
  EXPECT_EQ(0, memcmp(buffer, dest, to_read));
}

TEST(BufferReaderTest, Read_MoreThanRemaining) {
  const uint8_t buffer[] = {0x0, 0x1, 0x2, 0x3};
  constexpr const size_t buffer_size = sizeof(buffer);
  BufferReader reader(buffer, buffer_size);

  uint8_t dest[buffer_size * 2];
  constexpr const size_t dest_size = sizeof(dest);
  ASSERT_EQ(buffer_size, reader.Read(dest, dest_size));
  EXPECT_TRUE(reader.empty());
  EXPECT_EQ(0u, reader.BytesRemaining());
  EXPECT_EQ(0, memcmp(buffer, dest, buffer_size));
}

TEST(BufferReaderTest, Read_WhenEmpty) {
  BufferReader reader;
  ASSERT_TRUE(reader.empty());

  constexpr const size_t dest_size = 16;
  uint8_t dest[dest_size];
  EXPECT_EQ(0u, reader.Read(dest, dest_size));
  EXPECT_TRUE(reader.empty());
}

TEST(BufferReaderTest, ReadInteger_BigEndian) {
  const uint8_t buffer[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
  BufferReader reader(buffer, sizeof(buffer));

  EXPECT_EQ(0x01020304u, reader.ReadUint32());
  EXPECT_EQ(0x05060708u, reader.ReadUint32());
}

TEST(BufferReaderTest, ReadInteger_LittleEndian) {
  const uint8_t buffer[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
  BufferReader reader(buffer, sizeof(buffer));

  EXPECT_EQ(0x04030201u, reader.ReadUint32(kLittleEndian));
  EXPECT_EQ(0x08070605u, reader.ReadUint32(kLittleEndian));
}

TEST(BufferReaderTest, ReadInteger_NotEnoughDataBigEndian) {
  const uint8_t buffer[] = {0x1, 0x2};
  BufferReader reader(buffer, sizeof(buffer));

  EXPECT_EQ(0x01020000u, reader.ReadUint32());
}

TEST(BufferReaderTest, ReadInteger_NotEnoughDataLittleEndian) {
  const uint8_t buffer[] = {0x1, 0x2};
  BufferReader reader(buffer, sizeof(buffer));

  EXPECT_EQ(0x00000201u, reader.ReadUint32(kLittleEndian));
}

}  // namespace util
}  // namespace shaka
