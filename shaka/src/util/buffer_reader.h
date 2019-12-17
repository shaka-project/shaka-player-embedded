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

#ifndef SHAKA_EMBEDDED_UTIL_BUFFER_READER_H_
#define SHAKA_EMBEDDED_UTIL_BUFFER_READER_H_

#include <stddef.h>
#include <stdint.h>

namespace shaka {
namespace util {

enum Endianness {
  kBigEndian,
  kLittleEndian,

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  kHostOrder = kLittleEndian,
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  kHostOrder = kBigEndian,
#else
#  error "Invalid byte order"
#endif
};

/**
 * A simple utility class to read bytes from a buffer.  This does not own the
 * data and is not thread safe.
 */
class BufferReader {
 public:
  BufferReader();
  BufferReader(const uint8_t* data, size_t data_size);

  bool empty() const {
    return size_ == 0;
  }

  /** @return The number of bytes left to read. */
  size_t BytesRemaining() const {
    return size_;
  }

  /** Resets the buffer that this type will read from. */
  void SetBuffer(const uint8_t* data, size_t data_size);

  /**
   * Reads up to |dest_size| bytes and copies them into |dest|.
   * @param dest The buffer to copy the data to.
   * @param dest_size The number of bytes in |dest|.
   * @return The number of bytes read.
   */
  size_t Read(uint8_t* dest, size_t dest_size);

  /**
   * Skips the given number of bytes.
   * @return The number of bytes skipped.
   */
  size_t Skip(size_t count);

  /**
   * Reads a 8-bit integer from the buffer.  If the reader is empty, this
   * returns 0.
   */
  uint8_t ReadUint8() {
    return static_cast<uint8_t>(ReadInteger(1, kBigEndian));
  }

  /**
   * Reads a 32-bit integer from the buffer.  If there aren't enough bytes, this
   * will fill remaining bytes with 0s.  For example, in big-endian, if this
   * can only read two bytes {0x12, 0x34}, this will return 0x12340000.
   */
  uint32_t ReadUint32(Endianness endianness = kBigEndian) {
    return static_cast<uint32_t>(ReadInteger(4, endianness));
  }

 private:
  uint64_t ReadInteger(size_t size, Endianness endianness);

  const uint8_t* data_;
  size_t size_;
};

}  // namespace util
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTIL_BUFFER_READER_H_
