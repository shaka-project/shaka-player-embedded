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

#include <glog/logging.h>

#include <cstring>

namespace shaka {
namespace util {

BufferWriter::BufferWriter(uint8_t* data, size_t data_size)
    : data_(data), size_(data_size) {}

void BufferWriter::Write(const void* src, size_t src_size) {
  CHECK(size_ >= src_size) << "No output remaining";
  std::memcpy(data_, src, src_size);
  data_ += src_size;
  size_ -= src_size;
}

}  // namespace util
}  // namespace shaka
