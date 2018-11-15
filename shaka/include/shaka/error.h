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

#ifndef SHAKA_EMBEDDED_ERROR_H_
#define SHAKA_EMBEDDED_ERROR_H_

#include <iostream>
#include <memory>
#include <string>

#include "macros.h"

namespace shaka {

/**
 * @ingroup player
 */
enum class ErrorType : uint8_t {
  /**
   * A Shaka error was thrown.  See the |category| and |code| members for the
   * more specific error type.
   */
  ShakaError,

  /**
   * The required JavaScript member was missing or of an incorrect type.  This
   * can happen if the Shaka Player compiled script is incompatible with this
   * library.
   */
  BadMember,
  /**
   * A JavaScript exception was thrown, but it wasn't a Shaka error object.  See
   * the logs for more info.
   */
  NonShakaError,
};

/**
 * Represents a Player error.  This can be either a Shaka error or a more
 * generic JavaScript error.
 *
 * @see https://github.com/google/shaka-player/blob/master/lib/util/error.js
 * @ingroup player
 */
class SHAKA_EXPORT Error final {
 public:
  /** Creates an object with the given error category and code. */
  Error(ErrorType type, const std::string& message);
  /** Creates an object with the given error category and code. */
  Error(int category, int code, int severity, const std::string& message);

  Error(const Error&);
  Error(Error&&);
  ~Error();

  Error& operator=(const Error&);
  Error& operator=(Error&&);


  // TODO: Consider adding the data field.  Or consider adding a PrintError
  // method to print it without requiring the app to depend on V8.

  /** The error message. */
  std::string message;
  /** The kind of error. */
  ErrorType type;
  /**
   * The category of the error, if this is a Shaka error.  This is the same as
   * shaka.util.Error.Category.
   */
  int category;
  /**
   * The specific code of the error, if this is a Shaka error.  This is the same
   * as shaka.util.Error.Code.
   */
  int code;
  /**
   * The Shaka severity of the error, if this is a Shaka error.  This is the
   * same as shaka.util.Error.Severity.
   *
   * Only available in Shaka Player v2.1.0+.
   */
  int severity;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

inline std::ostream& operator<<(std::ostream& os, const Error& error) {
  return os << error.message;
}

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_ERROR_H_
