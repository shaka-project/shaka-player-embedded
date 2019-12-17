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

#ifndef SHAKA_EMBEDDED_UTIL_BUFFER_WRITER_H_
#define SHAKA_EMBEDDED_UTIL_BUFFER_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <type_traits>
#include <vector>

#include "src/util/buffer_reader.h"

namespace shaka {
namespace util {

/**
 * A simple utility class to write bytes to a buffer.  This does not own the
 * data and is not thread safe.
 *
 * This will fail a CHECK if a write would extend past the end of the buffer.
 */
class BufferWriter {
 public:
  BufferWriter(uint8_t* data, size_t data_size);

  bool empty() const {
    return size_ == 0;
  }

  /** @return The number of bytes left to write. */
  size_t BytesRemaining() const {
    return size_;
  }

  /** Writes a single byte. */
  void WriteByte(uint8_t byte) {
    Write(&byte, 1);
  }

  /** Writes the given big-endian MP4 tag. */
  void WriteTag(const char (&tag)[5]) {
    // Note this is char[5] since the null-terminator is included.
    Write(tag, 4);
  }

  /** Writes the given number. */
  template <typename T, typename = typename std::enable_if<
                            std::is_integral<T>::value>::type>
  void Write(T value, Endianness endian = kBigEndian) {
    if (endian == kHostOrder) {
      Write(&value, sizeof(value));
    } else {
      for (size_t i = 0; i < sizeof(T); i++) {
        if (endian == kBigEndian)
          WriteByte((value >> ((sizeof(T) - i - 1) * 8)) & 0xff);
        else
          WriteByte((value >> (i * 8)) & 0xff);
      }
    }
  }

  /** Writes the given vector. */
  void Write(const std::vector<uint8_t>& data) {
    Write(data.data(), data.size());
  }

  /**
   * Writes |src_size| bytes and copies them from |src|.
   * @param src The buffer to copy the data from.
   * @param src_size The number of bytes in |src|.
   */
  void Write(const void* src, size_t src_size);

 private:
  uint8_t* data_;
  size_t size_;
};

}  // namespace util
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTIL_BUFFER_WRITER_H_
