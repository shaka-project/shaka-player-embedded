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

#ifndef SHAKA_EMBEDDED_JS_DOM_DOM_EXCEPTION_H_
#define SHAKA_EMBEDDED_JS_DOM_DOM_EXCEPTION_H_

#include <string>

#include "shaka/optional.h"
#include "src/js/dom/exception_code.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"

namespace shaka {
namespace js {
namespace dom {

class DOMException : public BackingObject {
  DECLARE_TYPE_INFO(DOMException);

 public:
  explicit DOMException(ExceptionCode type);
  DOMException(optional<std::string> message, ExceptionCode type);

  static DOMException* Create(optional<std::string> message,
                              optional<std::string> name) {
    return new DOMException(message, name);
  }

  // NOTE: Should be labelled "name", but it conflicts with type info.
  const std::string error_name;
  const std::string message;
  std::string stack;
  // Legacy numeric code.
  const int code;

 private:
  DOMException(optional<std::string> message, optional<std::string> name);
};

class DOMExceptionFactory : public BackingObjectFactory<DOMException> {
 public:
  DOMExceptionFactory();
};

}  // namespace dom
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_DOM_DOM_EXCEPTION_H_
