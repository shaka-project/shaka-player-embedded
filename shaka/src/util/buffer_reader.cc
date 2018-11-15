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
#include <cstring>

namespace shaka {
namespace util {

BufferReader::BufferReader() : data_(nullptr), size_(0) {}
BufferReader::BufferReader(const uint8_t* data, size_t data_size)
    : data_(data), size_(data_size) {}

void BufferReader::SetBuffer(const uint8_t* data, size_t data_size) {
  data_ = data;
  size_ = data_size;
}

size_t BufferReader::Read(uint8_t* dest, size_t dest_size) {
  const uint8_t* src = data_;
  const size_t to_read = Skip(dest_size);
  std::memcpy(dest, src, to_read);
  return to_read;
}

size_t BufferReader::Skip(size_t count) {
  const size_t to_read = std::min(size_, count);
  data_ += to_read;
  size_ -= to_read;
  return to_read;
}

uint64_t BufferReader::ReadInteger(size_t size, Endianness endianness) {
  DCHECK(size == 1 || size == 2 || size == 4 || size == 8);
  uint64_t ret = 0;
  const size_t to_read = std::min(size, size_);
  for (size_t i = 0; i < to_read; i++) {
    if (endianness == kBigEndian)
      ret |= data_[i] << ((size - i - 1) * 8);
    else
      ret |= data_[i] << (i * 8);
  }

  data_ += to_read;
  size_ -= to_read;
  return ret;
}

}  // namespace util
}  // namespace shaka
