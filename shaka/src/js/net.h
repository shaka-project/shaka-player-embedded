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

#ifndef SHAKA_EMBEDDED_JS_NET_H_
#define SHAKA_EMBEDDED_JS_NET_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "shaka/net.h"
#include "shaka/optional.h"
#include "src/mapping/byte_buffer.h"
#include "src/mapping/enum.h"
#include "src/mapping/struct.h"

namespace shaka {
namespace js {

struct Request : public Struct {
  DECLARE_STRUCT_SPECIAL_METHODS_MOVE_ONLY(Request);

  ADD_DICT_FIELD(uris, std::vector<std::string>);
  ADD_DICT_FIELD(method, std::string);
  ADD_DICT_FIELD(headers, std::unordered_map<std::string, std::string>);
  ADD_DICT_FIELD(body, optional<ByteBuffer>);
};

struct Response : public Struct {
  DECLARE_STRUCT_SPECIAL_METHODS_MOVE_ONLY(Response);

  ADD_DICT_FIELD(uri, std::string);
  ADD_DICT_FIELD(originalUri, std::string);
  ADD_DICT_FIELD(headers, std::unordered_map<std::string, std::string>);
  ADD_DICT_FIELD(timeMs, optional<double>);
  ADD_DICT_FIELD(fromCache, optional<bool>);
  ADD_DICT_FIELD(data, ByteBuffer);
};

}  // namespace js
}  // namespace shaka

CONVERT_ENUM_AS_NUMBER(shaka, RequestType);

#endif  // SHAKA_EMBEDDED_JS_NET_H_
