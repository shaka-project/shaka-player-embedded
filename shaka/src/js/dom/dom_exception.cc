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

constexpr const char* kDefaultErrorName = "Error";

// See: https://www.w3.org/TR/WebIDL-1/#error-names
#define DEFINE_MAPPING(code, msg, number) \
  { #code, msg, code, number }
struct ExceptionInfo {
  const char* name;
  const char* message;
  ExceptionCode type;
  int native_code;
} g_exception_map_[] = {
    DEFINE_MAPPING(IndexSizeError, "The index is not in the allowed range.", 1),
    DEFINE_MAPPING(HierarchyRequestError,
                   "The operation would yield an incorrect node tree.", 3),
    DEFINE_MAPPING(WrongDocumentError,
                   "The object is in the wrong Document.", 4),
    DEFINE_MAPPING(InvalidCharacterError,
                   "The string contains invalid characters.", 5),
    DEFINE_MAPPING(NoModificationAllowedError,
                   "The object cannot be modified.", 7),
    DEFINE_MAPPING(NotFoundError, "The object can not be found here.", 8),
    DEFINE_MAPPING(NotSupportedError, "The operation is not supported.", 9),
    DEFINE_MAPPING(InUseAttributeError, "The attribute is in use.", 10),
    DEFINE_MAPPING(InvalidStateError, "The object is in an invalid state.", 11),
    DEFINE_MAPPING(SyntaxError,
                   "The string did not match the expected pattern.", 12),
    DEFINE_MAPPING(InvalidModificationError,
                   "The object cannot be modified.", 13),
    DEFINE_MAPPING(NamespaceError,
                   "The operation is not allowed by Namespaces in XML.", 14),
    DEFINE_MAPPING(InvalidAccessError,
                   "The object does not support the operation or argument.",
                   15),
    DEFINE_MAPPING(TypeMismatchError,
                   "The type of the object does not match the expected type.",
                   17),
    DEFINE_MAPPING(SecurityError, "The operation is insecure.", 18),
    DEFINE_MAPPING(NetworkError, "A network error occurred.", 19),
    DEFINE_MAPPING(AbortError, "The operation was aborted.", 20),
    DEFINE_MAPPING(URLMismatchError,
                   "The given URL does not match another URL.", 21),
    DEFINE_MAPPING(QuotaExceededError, "The quota has been exceeded.", 22),
    DEFINE_MAPPING(TimeoutError, "The operation timed out.", 23),
    DEFINE_MAPPING(InvalidNodeTypeError,
                   "The node is incorrect or has an incorrect ancestor for " \
                   "this operation.", 24),
    DEFINE_MAPPING(DataCloneError, "The object can not be cloned.", 25),
    DEFINE_MAPPING(EncodingError,
                   "The encoding or decoding operation failed.", 0),
    DEFINE_MAPPING(NotReadableError,
                   "The input/output read operation failed.", 0),
    DEFINE_MAPPING(UnknownError,
                   "The operation failed for an unknown transient reason " \
                   "(e.g. out of memory).", 0),
    DEFINE_MAPPING(ConstraintError,
                   "A mutation operation in a transaction failed because a " \
                   "constraint was not satisfied.", 0),
    DEFINE_MAPPING(DataError, "Provided data is inadequate.", 0),
    DEFINE_MAPPING(TransactionInactiveError,
                   "A request was placed against a transaction which is " \
                   "currently not active, or which is finished.", 0),
    DEFINE_MAPPING(ReadOnlyError,
                   "The mutating operation was attempted in a \"readonly\" " \
                   "transaction.", 0),
    DEFINE_MAPPING(VersionError,
                   "An attempt was made to open a database using a lower " \
                   "version than the existing version.", 0),
    DEFINE_MAPPING(OperationError,
                   "The operation failed for an operation-specific reason.", 0),
    DEFINE_MAPPING(NotAllowedError,
                   "The request is not allowed by the user agent or the " \
                   "platform in the current context, possibly because the " \
                   "user denied permission.", 0)
};
static_assert(sizeof(g_exception_map_) / sizeof(g_exception_map_[0]) ==
                  MaxExceptionCode,
              "Not all exceptions appear in map");
#undef DEFINE_MAPPING

const ExceptionInfo& GetInfo(ExceptionCode type) {
  for (const auto& except : g_exception_map_) {
    if (except.type == type) {
      return except;
    }
  }
  LOG(FATAL) << "Unknown exception type: " << type;
}

const ExceptionInfo* GetInfoByName(optional<std::string> name) {
  if (!name.has_value()) {
    return nullptr;
  }

  for (const auto& except : g_exception_map_) {
    if (except.name == name) {
      return &except;
    }
  }

  return nullptr;
}

int GetNativeCode(optional<std::string> name) {
  const ExceptionInfo* except = GetInfoByName(name);
  return except ? except->native_code : 0;
}

std::string GetDefaultMessage(optional<std::string> name) {
  const ExceptionInfo* except = GetInfoByName(name);
  return except ? except->message : "";
}

}  // namespace

DOMException::DOMException(ExceptionCode type)
    : error_name(GetInfo(type).name),
      message(GetInfo(type).message),
      code(GetInfo(type).native_code) {}

DOMException::DOMException(optional<std::string> message, ExceptionCode type)
    : error_name(GetInfo(type).name),
      message(message.value_or(GetInfo(type).message)),
      code(GetInfo(type).native_code) {}

DOMException::DOMException(optional<std::string> message,
                           optional<std::string> name)
    : error_name(name.value_or(kDefaultErrorName)),
      message(message.value_or(GetDefaultMessage(name))),
      code(GetNativeCode(name)) {}

// \cond Doxygen_Skip
DOMException::~DOMException() {}
// \endcond Doxygen_Skip

// static
DOMException* DOMException::Create(optional<std::string> message,
                                   optional<std::string> name) {
  return new DOMException(message, name);
}

DOMExceptionFactory::DOMExceptionFactory() {
  AddConstant("INDEX_SIZE_ERR", GetInfo(IndexSizeError).native_code);
  AddConstant("HIERARCHY_REQUEST_ERR",
              GetInfo(HierarchyRequestError).native_code);
  AddConstant("WRONG_DOCUMENT_ERR", GetInfo(WrongDocumentError).native_code);
  AddConstant("INVALID_CHARACTER_ERR",
              GetInfo(InvalidCharacterError).native_code);
  AddConstant("NO_MODIFICATION_ALLOWED_ERR",
              GetInfo(NoModificationAllowedError).native_code);
  AddConstant("NOT_FOUND_ERR", GetInfo(NotFoundError).native_code);
  AddConstant("INUSE_ATTRIBUTE_ERR", GetInfo(InUseAttributeError).native_code);
  AddConstant("NOT_SUPPORTED_ERR", GetInfo(NotSupportedError).native_code);
  AddConstant("INVALID_STATE_ERR", GetInfo(InvalidStateError).native_code);
  AddConstant("SYNTAX_ERR", GetInfo(SyntaxError).native_code);
  AddConstant("INVALID_MODIFICATION_ERR",
              GetInfo(InvalidModificationError).native_code);
  AddConstant("NAMESPACE_ERR", GetInfo(NamespaceError).native_code);
  AddConstant("INVALID_ACCESS_ERR", GetInfo(InvalidAccessError).native_code);
  AddConstant("TYPE_MISMATCH_ERR", GetInfo(TypeMismatchError).native_code);
  AddConstant("SECURITY_ERR", GetInfo(SecurityError).native_code);
  AddConstant("NETWORK_ERR", GetInfo(NetworkError).native_code);
  AddConstant("ABORT_ERR", GetInfo(AbortError).native_code);
  AddConstant("URL_MISMATCH_ERR", GetInfo(URLMismatchError).native_code);
  AddConstant("QUOTA_EXCEEDED_ERR", GetInfo(QuotaExceededError).native_code);
  AddConstant("TIMEOUT_ERR", GetInfo(TimeoutError).native_code);
  AddConstant("INVALID_NODE_TYPE_ERR",
              GetInfo(InvalidNodeTypeError).native_code);
  AddConstant("DATA_CLONE_ERR", GetInfo(DataCloneError).native_code);

  AddReadOnlyProperty("name", &DOMException::error_name);
  AddReadOnlyProperty("message", &DOMException::message);
  AddReadOnlyProperty("code", &DOMException::code);
  AddReadOnlyProperty("stack", &DOMException::stack);
}

}  // namespace dom
}  // namespace js
}  // namespace shaka
