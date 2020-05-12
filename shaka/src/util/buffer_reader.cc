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

#include <glog/logging.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace shaka {
namespace util {

BufferReader::BufferReader() : data_(nullptr), size_(0), bit_offset_(0) {}
BufferReader::BufferReader(const uint8_t* data, size_t data_size)
    : data_(data), size_(data_size), bit_offset_(0) {}

void BufferReader::SetBuffer(const uint8_t* data, size_t data_size) {
  data_ = data;
  size_ = data_size;
  bit_offset_ = 0;
}

size_t BufferReader::Read(uint8_t* dest, size_t dest_size) {
  if (bit_offset_ == 0) {
    const uint8_t* src = data_;
    const size_t to_read = Skip(dest_size);
    std::memcpy(dest, src, to_read);
    return to_read;
  } else {
    size_t i = 0;
    while (!empty()) {
      dest[i] = static_cast<uint8_t>(ReadBits(8));
      i++;
    }
    return i;
  }
}

size_t BufferReader::SkipBits(size_t count) {
  const size_t capacity = size_ * 8 - bit_offset_;
  const size_t to_read = std::min(count, capacity);
  const size_t to_read_bytes = (bit_offset_ + to_read) / 8;
  DCHECK_LE(to_read_bytes, size_);
  data_ += to_read_bytes;
  size_ -= to_read_bytes;
  bit_offset_ = (bit_offset_ + to_read) % 8;
  return to_read;
}

uint64_t BufferReader::ReadBits(size_t count, Endianness endianness) {
  DCHECK_LE(count, 64);
  DCHECK(endianness == kBigEndian || (bit_offset_ == 0 && count % 8 == 0));
  uint64_t ret = 0;

  for (size_t i = 0; i < count && size_ > 0;) {
    // Read a single byte, only keeping a part of it for sub-byte reads.
    // Sub-byte reads are always big-endian.
    //  0 1 1 0   1 1 0 1
    // |bit_offset_| c |  c=count
    //               |
    //       result=0b10
    const size_t bits_remain = 8 - bit_offset_;
    const size_t to_read = std::min(count - i, bits_remain);
    const size_t shift = bits_remain - to_read;
    const size_t mask = ((1 << to_read) - 1);
    const uint8_t part = (data_[0] >> shift) & mask;
    if (endianness == kBigEndian)
      ret |= part << (count - i - to_read);
    else
      ret |= part << i;

    if (bit_offset_ + to_read == 8) {
      data_++;
      size_--;
      bit_offset_ = 0;
    } else {
      DCHECK_LT(bit_offset_ + to_read, 8);
      bit_offset_ += to_read;
    }
    i += to_read;
  }

  return ret;
}

uint64_t BufferReader::ReadExpGolomb() {
  // See https://en.wikipedia.org/wiki/Exponential-Golomb_coding
  size_t count = 0;
  while (!empty() && ReadBits(1) == 0)
    count++;
  return (1 << count) - 1 + ReadBits(count);
}

}  // namespace util
}  // namespace shaka
