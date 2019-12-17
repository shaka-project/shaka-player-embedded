// Copyright 2019 Google LLC
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

#include "src/util/buffer_writer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/util/macros.h"

namespace shaka {
namespace util {

TEST(BufferWriterTest, BasicFlow) {
  const uint8_t input[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
  constexpr const size_t buffer_size = sizeof(input);
  uint8_t buffer[buffer_size];
  BufferWriter writer(buffer, buffer_size);
  ASSERT_FALSE(writer.empty());
  ASSERT_EQ(buffer_size, writer.BytesRemaining());

  writer.Write(input, sizeof(input));

  EXPECT_EQ(0u, writer.BytesRemaining());
  EXPECT_TRUE(writer.empty());
  EXPECT_EQ(0, memcmp(buffer, input, buffer_size));
}

TEST(BufferWriterTest, MultipleWrites) {
  const uint8_t input1[] = {0x2, 0x3, 0x4, 0x5};
  const uint8_t input2[] = {0x7, 0x8, 0x9, 0xa};
  const uint8_t expected[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5,
                              0x6, 0x7, 0x8, 0x9, 0xa};
  constexpr const size_t buffer_size = sizeof(expected);
  uint8_t buffer[buffer_size];
  BufferWriter writer(buffer, buffer_size);
  ASSERT_FALSE(writer.empty());
  ASSERT_EQ(buffer_size, writer.BytesRemaining());

  writer.Write<uint16_t>(0x0001, kBigEndian);
  writer.Write(input1, sizeof(input1));
  writer.WriteByte(6);
  writer.Write(input2, sizeof(input2));

  EXPECT_EQ(0u, writer.BytesRemaining());
  EXPECT_TRUE(writer.empty());
  EXPECT_EQ(0, memcmp(buffer, expected, buffer_size));
}

TEST(BufferWriterTest, WritesIntegers) {
  const uint8_t expected[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,  0x8, 0x9,
                              0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11};
  constexpr const size_t buffer_size = sizeof(expected);
  uint8_t buffer[buffer_size];
  BufferWriter writer(buffer, buffer_size);
  ASSERT_FALSE(writer.empty());
  ASSERT_EQ(buffer_size, writer.BytesRemaining());

  writer.Write<uint8_t>(0x01);
  writer.Write<uint16_t>(0x0203);  // Big endian
  writer.Write<uint16_t>(0x0504, kLittleEndian);
  writer.Write<uint32_t>(0x06070809, kBigEndian);
  writer.Write<uint64_t>(0x11100f0e0d0c0b0a, kLittleEndian);

  EXPECT_EQ(0u, writer.BytesRemaining());
  EXPECT_TRUE(writer.empty());
  EXPECT_EQ(0, memcmp(buffer, expected, buffer_size));
}

TEST(BufferWriterTest, WriteTag) {
  const uint8_t expected[] = {0x1, 0x2, 'p', 's', 's', 'h'};
  constexpr const size_t buffer_size = sizeof(expected);
  uint8_t buffer[buffer_size];
  BufferWriter writer(buffer, buffer_size);
  ASSERT_FALSE(writer.empty());
  ASSERT_EQ(buffer_size, writer.BytesRemaining());

  writer.Write<uint16_t>(0x0102);
  writer.WriteTag("pssh");

  EXPECT_EQ(0u, writer.BytesRemaining());
  EXPECT_TRUE(writer.empty());
  EXPECT_EQ(0, memcmp(buffer, expected, buffer_size));
}

#ifdef GTEST_HAS_DEATH_TEST
TEST(BufferWriterDeathTest, DoesntOverflow) {
  const uint8_t input[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6};
  constexpr const size_t buffer_size = 3;
  uint8_t buffer[buffer_size];
  BufferWriter writer(buffer, buffer_size);

  writer.Write<uint16_t>(10);
  EXPECT_DEATH(writer.Write<uint32_t>(0), "No output");
  EXPECT_DEATH(writer.Write(input, sizeof(input)), "No output");
  writer.WriteByte(0);
  EXPECT_DEATH(writer.WriteByte(0), "No output");

  EXPECT_TRUE(writer.empty());
}
#endif

}  // namespace util
}  // namespace shaka
