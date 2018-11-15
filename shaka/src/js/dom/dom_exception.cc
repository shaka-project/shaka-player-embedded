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

#include "src/js/dom/dom_exception.h"

#include <glog/logging.h>

#include <unordered_map>

#include "src/util/macros.h"

namespace shaka {
namespace js {
namespace dom {

namespace {

// See: https://www.w3.org/TR/WebIDL-1/#error-names
#define DEFINE_MAPPING(code, msg, number) \
  { #code, msg, code, number }
struct ExceptionInfo {
  const char* name;
  const char* message;
  ExceptionCode type;
  int native_code;
} g_exception_map_[] = {
    DEFINE_MAPPING(NotFoundError, "The object can not be found here.", 8),
    DEFINE_MAPPING(NotSupportedError, "The operation is not supported.", 9),
    DEFINE_MAPPING(InvalidStateError, "The object is in an invalid state.", 11),
    DEFINE_MAPPING(QuotaExceededError, "The quota has been exceeded.", 22),

    DEFINE_MAPPING(IndexSizeError, "The index is not in the allowed range.", 1),
    DEFINE_MAPPING(HierarchyRequestError,
                   "The operation would yield an incorrect node tree.", 3),

    DEFINE_MAPPING(DataCloneError, "The object can not be cloned.", 25),
    DEFINE_MAPPING(UnknownError,
                   "The operation failed for an unknown transient reason (e.g. "
                   "out of memory).",
                   0),
    DEFINE_MAPPING(TransactionInactiveError,
                   "A request was placed against a transaction which is "
                   "currently not active, or which is finished.",
                   0),
    DEFINE_MAPPING(
        ReadOnlyError,
        "The mutating operation was attempted in a \"readonly\" transaction.",
        0),
    DEFINE_MAPPING(VersionError,
                   "An attempt was made to open a database using a lower "
                   "version than the existing version.",
                   0),
};
#undef DEFINE_MAPPING

const ExceptionInfo& GetInfo(ExceptionCode type) {
  for (const auto& except : g_exception_map_) {
    if (except.type == type)
      return except;
  }
  LOG(FATAL) << "Unknown exception type: " << type;
}

}  // namespace

DOMException::DOMException(ExceptionCode type)
    : error_name(GetInfo(type).name),
      message(GetInfo(type).message),
      code(GetInfo(type).native_code) {}

DOMException::DOMException(ExceptionCode type, const std::string& message)
    : error_name(GetInfo(type).name),
      message(message),
      code(GetInfo(type).native_code) {}

DOMException::DOMException(const std::string& name,
                           optional<std::string> message)
    : error_name(name), message(message.value_or("")), code(0) {}

// \cond Doxygen_Skip
DOMException::~DOMException() {}
// \endcond Doxygen_Skip

DOMExceptionFactory::DOMExceptionFactory() {
  AddReadOnlyProperty("name", &DOMException::error_name);
  AddReadOnlyProperty("message", &DOMException::message);
  AddReadOnlyProperty("code", &DOMException::code);
  AddReadOnlyProperty("stack", &DOMException::stack);
}

}  // namespace dom
}  // namespace js
}  // namespace shaka
