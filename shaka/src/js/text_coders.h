// Copyright 2020 Google LLC
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

#ifndef SHAKA_EMBEDDED_JS_TEXT_CODERS_H_
#define SHAKA_EMBEDDED_JS_TEXT_CODERS_H_

#include <string>

#include "shaka/optional.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/byte_buffer.h"
#include "src/mapping/enum.h"
#include "src/mapping/exception_or.h"
#include "src/mapping/struct.h"

namespace shaka {
namespace js {

enum class TextEncoding {
  UTF8,
};

struct TextDecoderOptions : public Struct {
  DECLARE_STRUCT_SPECIAL_METHODS_COPYABLE(TextDecoderOptions);

  ADD_DICT_FIELD(fatal, bool);
};

class TextDecoder : public BackingObject {
  DECLARE_TYPE_INFO(TextDecoder);

 public:
  explicit TextDecoder(TextEncoding encoding);

  const TextEncoding encoding;
  const bool fatal = false;
  const bool ignoreBOM = true;

  static ExceptionOr<TextDecoder*> Create(
      optional<std::string> encoding, optional<TextDecoderOptions> options);

  ExceptionOr<std::string> Decode(ByteBuffer buffer);
};

class TextDecoderFactory : public BackingObjectFactory<TextDecoder> {
 public:
  TextDecoderFactory();
};


class TextEncoder : public BackingObject {
  DECLARE_TYPE_INFO(TextEncoder);

 public:
  TextEncoder();

  const TextEncoding encoding = TextEncoding::UTF8;

  static TextEncoder* Create() {
    return new TextEncoder;
  }

  ByteBuffer Encode(const std::string& str);
};

class TextEncoderFactory : public BackingObjectFactory<TextEncoder> {
 public:
  TextEncoderFactory();
};

}  // namespace js
}  // namespace shaka

DEFINE_ENUM_MAPPING(shaka::js, TextEncoding) {
  AddMapping(Enum::UTF8, "utf-8");
}

#endif  // SHAKA_EMBEDDED_JS_TEXT_CODERS_H_
