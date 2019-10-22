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

#include <algorithm>
#include <cstring>

namespace shaka {
namespace util {

const size_t DynamicBuffer::kMinBufferSize;

DynamicBuffer::DynamicBuffer() {}
DynamicBuffer::~DynamicBuffer() {}

DynamicBuffer::DynamicBuffer(DynamicBuffer&&) = default;
DynamicBuffer& DynamicBuffer::operator=(DynamicBuffer&&) = default;

size_t DynamicBuffer::Size() const {
  size_t size = 0;
  for (auto& buffer : buffers_)
    size += buffer.used;
  return size;
}

void DynamicBuffer::AppendCopy(const void* buffer, size_t size) {
  if (!buffers_.empty()) {
    auto* info = &buffers_.back();
    const size_t to_copy = std::min(info->capacity - info->used, size);
    std::memcpy(info->buffer.get() + info->used, buffer, to_copy);
    info->used += to_copy;
    buffer = reinterpret_cast<const uint8_t*>(buffer) + to_copy;
    size -= to_copy;
  }

  if (size > 0) {
    const size_t capacity = std::max(kMinBufferSize, size);
    auto* ptr = new uint8_t[capacity];
    std::memcpy(ptr, buffer, size);
    buffers_.emplace_back(ptr, size, capacity);
  }
}

std::string DynamicBuffer::CreateString() const {
  std::string ret(Size(), '\0');
  CopyDataTo(reinterpret_cast<uint8_t*>(&ret[0]), ret.size());
  return ret;
}

void DynamicBuffer::CopyDataTo(uint8_t* dest, size_t size) const {
  for (auto& buffer : buffers_) {
    CHECK_GE(size, buffer.used);
    std::memcpy(dest, buffer.buffer.get(), buffer.used);
    dest += buffer.used;
    size -= buffer.used;
  }
}

DynamicBuffer::SubBuffer::SubBuffer(uint8_t* buffer, size_t used,
                                    size_t capacity)
    : buffer(buffer), used(used), capacity(capacity) {}

DynamicBuffer::SubBuffer::~SubBuffer() {}

}  // namespace util
}  // namespace shaka
