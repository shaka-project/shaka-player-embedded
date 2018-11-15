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

#include "src/util/utils.h"

#include <glog/logging.h>

#include <algorithm>
#include <cctype>

namespace shaka {
namespace util {

namespace {

PRINTF_FORMAT(2, 0)
void StringAppendV(std::string* dest, const char* format, va_list ap) {
  // Get the required size of the buffer.
  va_list ap_copy;
  va_copy(ap_copy, ap);
  int size = vsnprintf(nullptr, 0, format, ap_copy);  // NOLINT
  va_end(ap_copy);

  PCHECK(size >= 0);
  std::string buffer(size + 1, '\0');
  CHECK_EQ(size, vsnprintf(&buffer[0], buffer.size(), format, ap));

  // Drop the '\0' that was added by vsnprintf, std::string has an implicit '\0'
  // after the string.
  buffer.resize(size);

  dest->append(buffer);
}

}  // namespace


std::string StringPrintf(const char* format, ...) {  // NOLINT(cert-dcl50-cpp)
  va_list ap;
  va_start(ap, format);
  std::string result;
  StringAppendV(&result, format, ap);
  va_end(ap);
  return result;
}

std::string StringPrintfV(const char* format, va_list va) {
  std::string result;
  StringAppendV(&result, format, va);
  return result;
}

std::vector<std::string> StringSplit(const std::string& source, char split_on) {
  std::vector<std::string> split;
  std::string::size_type start = 0;
  std::string::size_type end = 0;
  while ((end = source.find(split_on, start)) != std::string::npos) {
    split.push_back(source.substr(start, end - start));
    start = end + 1;
  }
  split.push_back(source.substr(start));
  return split;
}

std::string ToAsciiLower(const std::string& source) {
  std::string ret = source;
  std::transform(source.begin(), source.end(), ret.begin(), ::tolower);
  return ret;
}

std::string TrimAsciiWhitespace(const std::string& source) {
  size_t start = 0;
  size_t end = source.size();
  while (start < source.size() && std::isspace(source[start]))
    ++start;
  if (start == source.size())
    return "";

  while (end > 0 && std::isspace(source[end - 1]))
    --end;

  DCHECK_LT(start, end);
  return source.substr(start, end - start);
}

std::string ToHexString(const uint8_t* data, size_t data_size) {
  const char kCharMap[] = "0123456789ABCDEF";
  std::string ret(data_size * 2, '\0');
  DCHECK_EQ(ret.size(), data_size * 2);
  for (size_t i = 0; i < data_size; i++) {
    ret[i * 2] = kCharMap[data[i] >> 4];
    ret[i * 2 + 1] = kCharMap[data[i] & 0xf];
  }
  return ret;
}

}  // namespace util
}  // namespace shaka
