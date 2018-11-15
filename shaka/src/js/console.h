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

#ifndef SHAKA_EMBEDDED_JS_CONSOLE_H_
#define SHAKA_EMBEDDED_JS_CONSOLE_H_

#include <string>

#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/js_wrappers.h"

namespace shaka {
namespace js {

class Console : public BackingObject {
  DECLARE_TYPE_INFO(Console);

 public:
  enum LogLevel {
    kError,
    kWarn,
    kInfo,
    kLog,
    kDebug,
  };

  Console();

  void Assert(Any cond, const CallbackArguments& arguments) const;
  void Error(const CallbackArguments& arguments) const;
  void Warn(const CallbackArguments& arguments) const;
  void Info(const CallbackArguments& arguments) const;
  void Log(const CallbackArguments& arguments) const;
  void Debug(const CallbackArguments& arguments) const;

  /**
   * @return The given value concerted to a string. If the given value is an
   *   object, all fields and their values will be printed.
   */
  static std::string ConvertToPrettyString(Handle<JsValue> value);

 private:
  /**
   * @param level The log level to print at.
   * @param arguments The arguments to log.
   * @param prefix An optional prefix to prepend to the log.
   * @param skip_count The number of arguments to skip.
   */
  void LogReal(LogLevel level, const CallbackArguments& arguments,
               const char* prefix = nullptr, size_t skip_count = 0) const;
};

class ConsoleFactory : public BackingObjectFactory<Console> {
 public:
  ConsoleFactory();
  ~ConsoleFactory() override {}
};

}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_CONSOLE_H_
