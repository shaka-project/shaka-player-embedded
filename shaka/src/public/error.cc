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

#include "shaka/error.h"

#include "src/util/macros.h"

namespace shaka {

class Error::Impl {};

Error::Error(ErrorType type, const std::string& message)
    : message(message), type(type), category(0), code(0), severity(0) {}

Error::Error(int category, int code, int severity, const std::string& message)
    : message(message),
      type(ErrorType::ShakaError),
      category(category),
      code(code),
      severity(severity) {}

Error::Error(const Error& other)
    : message(other.message),
      type(other.type),
      category(other.category),
      code(other.code),
      severity(other.severity) {}

Error::Error(Error&&) = default;

Error::~Error() {}

Error& Error::operator=(const Error& other) {
  message = other.message;
  type = other.type;
  category = other.category;
  code = other.code;
  severity = other.severity;

  return *this;
}

Error& Error::operator=(Error&&) = default;

}  // namespace shaka
