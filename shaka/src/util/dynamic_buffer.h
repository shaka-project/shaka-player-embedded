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

#ifndef SHAKA_EMBEDDED_UTIL_DYNAMIC_BUFFER_H_
#define SHAKA_EMBEDDED_UTIL_DYNAMIC_BUFFER_H_

#include <list>
#include <memory>
#include <string>

namespace shaka {
namespace util {

/**
 * Represents a buffer of bytes that can be appended to without unnecessary
 * copies.  This does so by storing an array of the sub-buffers it stores.  This
 * means that you cannot get a singular data pointer.  There are helper methods
 * that can copy this to a contiguous buffer (e.g. std::string).
 */
class DynamicBuffer {
 public:
  DynamicBuffer();
  ~DynamicBuffer();

  DynamicBuffer(const DynamicBuffer&) = delete;
  DynamicBuffer(DynamicBuffer&&);
  DynamicBuffer& operator=(const DynamicBuffer&) = delete;
  DynamicBuffer& operator=(DynamicBuffer&&);

  /** @return The total size of the buffer, in bytes. */
  size_t Size() const;

  /** Clears the contents of the buffer. */
  void Clear() {
    buffers_.clear();
  }

  /** Appends to the buffer by copying the given data. */
  void AppendCopy(const void* buffer, size_t size);


  /** @return A new string that contains the data in the buffer. */
  std::string CreateString() const;

  /** Copies the contents of this buffer to the given buffer. */
  void CopyDataTo(uint8_t* dest, size_t size) const;

 private:
  struct SubBuffer {
    SubBuffer(uint8_t* buffer, size_t size);
    ~SubBuffer();

    std::unique_ptr<uint8_t[]> buffer;
    size_t size;
  };

  std::list<SubBuffer> buffers_;
};

}  // namespace util
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTIL_DYNAMIC_BUFFER_H_
