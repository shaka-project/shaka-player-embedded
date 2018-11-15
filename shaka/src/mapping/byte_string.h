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

#ifndef SHAKA_EMBEDDED_MAPPING_BYTE_STRING_H_
#define SHAKA_EMBEDDED_MAPPING_BYTE_STRING_H_

#include <string>
#include <vector>

#include "src/mapping/generic_converter.h"

namespace shaka {

/**
 * Represents a string where each character is interpreted as a byte,
 * independent of the encoding.
 *
 * This inherits from std::vector to expose the same methods (e.g. size(),
 * data(), operator[]).
 */
class ByteString : public std::vector<uint8_t>,
                   public GenericConverter,
                   public memory::Traceable {
 public:
  static std::string name() {
    return "string";
  }

  explicit ByteString(const char* source);
  explicit ByteString(const std::string& source);

  // Inherit other constructors from std::vector.
  using std::vector<uint8_t>::vector;


  bool TryConvert(Handle<JsValue> value) override;
  ReturnVal<JsValue> ToJsValue() const override;
  void Trace(memory::HeapTracer* tracer) const override {}
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_BYTE_STRING_H_
