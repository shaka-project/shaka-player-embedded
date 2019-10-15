// Copyright 2018 Google LLC
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

#ifndef SHAKA_EMBEDDED_EME_DATA_H_
#define SHAKA_EMBEDDED_EME_DATA_H_

#include <stddef.h>  // For size_t
#include <stdint.h>  // For uint8_t

#include <memory>

#include "../macros.h"

namespace shaka {

class ByteBuffer;

namespace js {
namespace eme {
class MediaKeys;
class MediaKeySession;
}  // namespace eme
}  // namespace js

namespace eme {

/**
 * Defines a wrapper around data passed into EME.  This type will keep the
 * backing data alive so long as this object is alive.
 *
 * This type is not thread-safe.
 *
 * @ingroup eme
 */
class SHAKA_EXPORT Data final {
 public:
  Data(const Data& other) = delete;
  Data(Data&& other);
  ~Data();

  Data& operator=(const Data& other) = delete;
  Data& operator=(Data&& other);

  /** @return A raw pointer to the data. */
  const uint8_t* data() const;
  /** @return The number of bytes in this data. */
  size_t size() const;

 private:
  friend class ClearKeyImplementationTest;
  friend class js::eme::MediaKeys;
  friend class js::eme::MediaKeySession;
  Data(ByteBuffer* buffer);

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace eme
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_EME_DATA_H_
