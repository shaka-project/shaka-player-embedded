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

#include "src/js/base_64.h"

#include "src/js/js_error.h"
#include "src/mapping/register_member.h"

namespace shaka {
namespace js {

namespace {

constexpr const char kCodes[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

// Gets the low |n| bits of |in|.
#define GET_LOW_BITS(in, n) ((in) & ((1 << (n)) - 1))
// Gets the given (zero-indexed) bits [a, b) of |in|.
#define GET_BITS(in, a, b) GET_LOW_BITS((in) >> (a), (b) - (a))
// Calculates a/b using round-up division (only works for positive numbers).
#define CEIL_DIVIDE(a, b) ((((a)-1) / (b)) + 1)

int32_t DecodeChar(char c) {
  const char* it = strchr(kCodes, c);
  if (it == nullptr)
    return -1;
  return it - kCodes;
}

JsError BadEncoding() {
  return JsError::TypeError(
      "The string to be decoded is not correctly encoded.");
}

}  // namespace

void Base64::Install() {
  RegisterGlobalFunction("btoa", &Base64::Encode);
  RegisterGlobalFunction("atob", &Base64::Decode);
}

// https://en.wikipedia.org/wiki/Base64
// Text    |       M        |       a       |       n        |
// ASCI    |   77 (0x4d)    |   97 (0x61)   |   110 (0x6e)   |
// Bits    | 0 1 0 0 1 1 0 1 0 1 1 0 0 0 0 1 0 1 1 0 1 1 1 0 |
// Index   |     19     |     22    |      5    |     46     |
// Base64  |      T     |      W    |      F    |      u     |
//         | <-----------------  24-bits  -----------------> |

std::string Base64::Encode(ByteString input) {
  if (input.empty())
    return "";

  // Stores a 24-bit block that is treated as an array where insertions occur
  // from high to low.
  uint32_t temp = 0;
  size_t out_i = 0;
  const size_t out_size = CEIL_DIVIDE(input.size(), 3) * 4;
  std::string result(out_size, '\0');
  for (size_t i = 0; i < input.size(); i++) {
    // "insert" 8-bits of data
    temp |= (input[i] << ((2 - (i % 3)) * 8));

    if (i % 3 == 2) {
      result[out_i++] = kCodes[GET_BITS(temp, 18, 24)];
      result[out_i++] = kCodes[GET_BITS(temp, 12, 18)];
      result[out_i++] = kCodes[GET_BITS(temp, 6, 12)];
      result[out_i++] = kCodes[GET_BITS(temp, 0, 6)];
      temp = 0;
    }
  }

  if (input.size() % 3 == 1) {
    result[out_i++] = kCodes[GET_BITS(temp, 18, 24)];
    result[out_i++] = kCodes[GET_BITS(temp, 12, 18)];
    result[out_i++] = '=';
    result[out_i++] = '=';
  } else if (input.size() % 3 == 2) {
    result[out_i++] = kCodes[GET_BITS(temp, 18, 24)];
    result[out_i++] = kCodes[GET_BITS(temp, 12, 18)];
    result[out_i++] = kCodes[GET_BITS(temp, 6, 12)];
    result[out_i++] = '=';
  }

  DCHECK_EQ(out_size, out_i);
  return result;
}

ExceptionOr<ByteString> Base64::Decode(const std::string& input) {
  if (input.empty())
    return "";

  const size_t out_size_max = CEIL_DIVIDE(input.size() * 3, 4);
  std::string result(out_size_max, '\0');

  // Stores 24-bits of data that is treated like an array where insertions occur
  // from high to low.
  uint32_t temp = 0;
  size_t out_i = 0;
  size_t i;
  for (i = 0; i < input.size(); i++) {
    if (input[i] == '=') {
      // We want i to remain at the first '=', so we need an inner loop.
      for (size_t j = i; j < input.size(); j++) {
        if (input[j] != '=')
          return BadEncoding();
      }
      break;
    }

    const int32_t decoded = DecodeChar(input[i]);
    if (decoded < 0)
      return BadEncoding();
    // "insert" 6-bits of data
    temp |= (decoded << ((3 - (i % 4)) * 6));

    if (i % 4 == 3) {
      result[out_i++] = GET_BITS(temp, 16, 24);
      result[out_i++] = GET_BITS(temp, 8, 16);
      result[out_i++] = GET_BITS(temp, 0, 8);
      temp = 0;
    }
  }

  switch (i % 4) {
    case 1:
      return BadEncoding();
    case 2:
      result[out_i++] = GET_BITS(temp, 16, 24);
      break;
    case 3:
      result[out_i++] = GET_BITS(temp, 16, 24);
      result[out_i++] = GET_BITS(temp, 8, 16);
      break;
  }
  result.resize(out_i);
  return result;
}

std::string Base64::EncodeUrl(ByteString input) {
  std::string ret = Encode(input);
  for (char& c : ret) {
    if (c == '+')
      c = '-';
    else if (c == '/')
      c = '_';
  }
  ret.erase(ret.find('='));  // Erase any trailing '='
  return ret;
}

ExceptionOr<ByteString> Base64::DecodeUrl(const std::string& input) {
  std::string converted_input = input;
  for (char& c : converted_input) {
    if (c == '-')
      c = '+';
    else if (c == '_')
      c = '/';
  }
  // Note Decode() will ignore any missing '='
  return Decode(converted_input);
}

}  // namespace js
}  // namespace shaka
