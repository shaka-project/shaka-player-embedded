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

#ifndef SHAKA_EMBEDDED_JS_BASE_64_H_
#define SHAKA_EMBEDDED_JS_BASE_64_H_

#include <string>

#include "src/mapping/byte_string.h"
#include "src/mapping/exception_or.h"

namespace shaka {
namespace js {

class Base64 {
 public:
  static void Install();

  static std::string Encode(ByteString input);
  static ExceptionOr<ByteString> Decode(const std::string& input);

  static std::string EncodeUrl(ByteString input);
  static ExceptionOr<ByteString> DecodeUrl(const std::string& input);
};

}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_BASE_64_H_
