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

#ifndef SHAKA_EMBEDDED_MAPPING_BYTE_BUFFER_H_
#define SHAKA_EMBEDDED_MAPPING_BYTE_BUFFER_H_

#include <cstring>
#include <string>

#include "src/mapping/generic_converter.h"
#include "src/mapping/js_wrappers.h"
#include "src/mapping/weak_js_ptr.h"
#include "src/util/dynamic_buffer.h"

namespace shaka {

/**
 * Represents a buffer of data that is shared between C++ and JavaScript.  This
 * holds a single ArrayBuffer that refers to the data, which can be passed into
 * JavaScript.  Since ArrayBuffers are immutable, this type is read-only.
 */
class ByteBuffer : public GenericConverter, public memory::Traceable {
 public:
  static std::string name() {
    return "arraybuffer";
  }

  ByteBuffer();
  ByteBuffer(const uint8_t* data, size_t data_size);
  ~ByteBuffer() override;

  ByteBuffer(const ByteBuffer&) = delete;
  ByteBuffer(ByteBuffer&&);
  ByteBuffer& operator=(const ByteBuffer& other) = delete;
  ByteBuffer& operator=(ByteBuffer&& other);


  /** @return A pointer to the buffer. */
  const uint8_t* data() const {
    return ptr_;
  }

  /** @return The size of the buffer (in bytes). */
  size_t size() const {
    return size_;
  }

  /** Clears all the data stored in the buffer. */
  void Clear();

  /**
   * Clears the buffer and sets the contents to a copy of the given buffer.
   * This can be called from any thread.
   *
   * This will allocate a block of memory the same as JavaScript expects and
   * take ownership of it (own_ptr_).  Then, when we need the ArrayBuffer, we
   * create it then giving up ownership of the pointer so we don't have to copy
   * it.
   */
  void SetFromDynamicBuffer(const util::DynamicBuffer& other);

  /** Similar to SetFromDynamicBuffer, except accepts a single buffer source. */
  void SetFromBuffer(const void* buffer, size_t size);


  bool TryConvert(Handle<JsValue> value) override;
  ReturnVal<JsValue> ToJsValue() const override;
  void Trace(memory::HeapTracer* tracer) const override;

 private:
  /** Resets all fields to empty. */
  void ClearFields();

  /**
   * Clears the buffer and allocates a new buffer of the given size.  This will
   * allocate the block the same as JavaScript expects and take ownership of
   * it (own_ptr_).  Then, when we need the ArrayBuffer, we create it then,
   * giving up ownership of the pointer so we don't have to copy it.
   */
  void ClearAndAllocateBuffer(size_t size);

  // Both |buffer_| and |ptr_| point to the same data block.  |buffer_| and
  // |own_ptr_| are mutable to allow ToJsValue to create a new ArrayBuffer to
  // take ownership of |ptr_|.
  mutable WeakJsPtr<JsObject> buffer_;
  uint8_t* ptr_ = nullptr;
  size_t size_ = 0;
  // Whether we own |ptr_|, this may be slightly different from
  // |buffer_.empty()| since the ArrayBuffer may be destroyed before we
  // are during a GC run.
  mutable bool own_ptr_ = false;
};

inline bool operator==(const ByteBuffer& lhs, const ByteBuffer& rhs) {
  return lhs.size() == rhs.size() &&
         memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}
inline bool operator!=(const ByteBuffer& lhs, const ByteBuffer& rhs) {
  return !(lhs == rhs);
}

}  // namespace shaka

namespace std {

template <>
struct hash<shaka::ByteBuffer> {
  size_t operator()(const shaka::ByteBuffer& buffer) const {
    // Introduce some noise to make small buffers have a more distributed hash.
    uint64_t ret = 0xacbdcfd0e1f20304;

    const uint8_t* ptr = buffer.data();
    for (size_t count = buffer.size(); count > 0; ptr++, count--) {
      // Rotate the number to make the order of bytes matter and so we affect
      // the whole number.
      ret = (ret << 8) | (ret >> 56);

      // "insert" the data into the hash.
      ret ^= *ptr;
    }

    // Intentionally truncate on 32-bit platforms.
    return static_cast<size_t>(ret);
  }
};

}  // namespace std

#endif  // SHAKA_EMBEDDED_MAPPING_BYTE_BUFFER_H_
