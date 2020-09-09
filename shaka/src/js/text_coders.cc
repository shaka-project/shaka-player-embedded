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

#include "src/js/text_coders.h"

#include "src/js/js_error.h"

namespace shaka {
namespace js {

namespace {

bool GetEncoding(const std::string& value, TextEncoding* encoding) {
  if (value == "unicode-1-1-utf-8" || value == "utf-8" || value == "utf8") {
    *encoding = TextEncoding::UTF8;
  } else {
    return false;
  }
  return true;
}

}  // namespace

DEFINE_STRUCT_SPECIAL_METHODS_COPYABLE(TextDecoderOptions);

TextDecoder::TextDecoder(TextEncoding encoding) : encoding(encoding) {}

// \cond Doxygen_Skip
TextDecoder::~TextDecoder() {}
// \endcond Doxygen_Skip

ExceptionOr<TextDecoder*> TextDecoder::Create(
    optional<std::string> encoding, optional<TextDecoderOptions> options) {
  TextEncoding parsed_encoding = TextEncoding::UTF8;
  if (encoding.has_value() &&
      !GetEncoding(encoding.value(), &parsed_encoding)) {
    return JsError::DOMException(NotSupportedError,
                                 "Unsupported encoding: " + encoding.value());
  }
  if (options.has_value() && options->fatal) {
    return JsError::DOMException(NotSupportedError,
                                 "Fatal decoder errors aren't supported");
  }

  return new TextDecoder(parsed_encoding);
}

ExceptionOr<std::string> TextDecoder::Decode(ByteBuffer buffer) {
  // std::string is already UTF-8, so just use the mapping framework to convert.
  return std::string{reinterpret_cast<const char*>(buffer.data()),
                     buffer.size()};
}


TextDecoderFactory::TextDecoderFactory() {
  AddReadOnlyProperty("encoding", &TextDecoder::encoding);
  AddReadOnlyProperty("fatal", &TextDecoder::fatal);
  AddReadOnlyProperty("ignoreBOM", &TextDecoder::ignoreBOM);

  AddMemberFunction("decode", &TextDecoder::Decode);
}


TextEncoder::TextEncoder() {}

// \cond Doxygen_Skip
TextEncoder::~TextEncoder() {}
// \endcond Doxygen_Skip

ByteBuffer TextEncoder::Encode(const std::string& str) {
  // std::string is already UTF-8, so just use the mapping framework to convert.
  return ByteBuffer{reinterpret_cast<const uint8_t*>(str.data()), str.size()};
}


TextEncoderFactory::TextEncoderFactory() {
  AddReadOnlyProperty("encoding", &TextEncoder::encoding);

  AddMemberFunction("encode", &TextEncoder::Encode);
}

}  // namespace js
}  // namespace shaka
