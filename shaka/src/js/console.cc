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

#include "src/js/console.h"

#include <algorithm>  // std::min and std::sort
#include <cstdio>
#include <vector>

#include "src/js/js_error.h"

namespace shaka {
namespace js {

namespace {

// Limit the number of children we are willing to print. This will limit the
// number of elements printed from arrays and the number of members printed
// from objects.
constexpr const size_t kMaxChildren = 20;

std::string ConvertToPrettyString(Handle<JsValue> value, bool allow_long);

std::string to_string(Console::LogLevel level) {
  switch (level) {
    case Console::kError:
      return "Error";
    case Console::kWarn:
      return "Warn";
    case Console::kInfo:
      return "Info";
    case Console::kLog:
      return "Log";
    case Console::kDebug:
      return "Debug";
  }
}

std::string ConvertStringToPrettyString(const std::string& string) {
  std::string buffer;
  buffer.push_back('"');
  for (const char c : string) {
    // Using https://en.wikipedia.org/wiki/Escape_sequences_in_C to determine
    // characters need to be escaped.
    switch (c) {
      // Alert
      case '\a':
        buffer += R"(\a)";
        break;
      // Backspace
      case '\b':
        buffer += R"(\b)";
        break;
      // Newline
      case '\n':
        buffer += R"(\n)";
        break;
      // Carriage Return
      case '\r':
        buffer += R"(\r)";
        break;
      // Horizontal Tab
      case '\t':
        buffer += R"(\t)";
        break;
      // Backslash
      case '\\':
        buffer += R"(\\)";
        break;
      // Single Quotation Mark
      case '\'':
        buffer += R"(\')";
        break;
      // Double Quotation Mark
      case '"':
        buffer += R"(\")";
        break;
      // Question Mark
      case '\?':
        buffer += R"(\?)";
        break;
      default:
        buffer.push_back(c);
        break;
    }
  }
  buffer.push_back('"');
  return buffer;
}

std::string ConvertArrayToLongPrettyString(Handle<JsValue> value) {
  const LocalVar<JsObject> array = UnsafeJsCast<JsObject>(value);
  const size_t array_length = ArrayLength(array);
  const size_t min_length = std::min(kMaxChildren, array_length);

  // TODO: Use a string buffer to build strings to avoid having to create
  //       strings just to concatenate them.
  std::string array_as_string = "[";

  for (size_t i = 0; i < min_length; i++) {
    const LocalVar<JsValue> index_value = GetArrayIndexRaw(array, i);

    array_as_string += i ? ", " : "";
    array_as_string += ConvertToPrettyString(index_value, false);
  }

  // If there are more elements than we want to print, tell the user by
  // appending a tail.
  if (array_length > min_length) {
    array_as_string += ", ...";
  }

  array_as_string += "]";

  return array_as_string;
}

std::string ConvertObjectToLongPrettyString(Handle<JsObject> object) {
  // Get the member names in sorted order so that it will be easier to
  // see which member are different between to objects when viewing the
  // output.
  std::vector<std::string> member_names = GetMemberNames(object);
  std::sort(member_names.begin(), member_names.end());

  const size_t min_length = std::min(kMaxChildren, member_names.size());

  std::string object_as_string = "{";

  for (size_t i = 0; i < min_length; i++) {
    const std::string& member_name = member_names[i];
    const LocalVar<JsValue> member_value = GetMemberRaw(object, member_name);

    object_as_string += i ? ", " : "";
    object_as_string += member_name;
    object_as_string += ":";
    object_as_string += ConvertToPrettyString(member_value, false);
  }

  // If there are more members than we want to print, tell the user by
  // appending a tail.
  if (member_names.size() > min_length) {
    object_as_string += ", ...";
  }

  object_as_string += "}";
  return object_as_string;
}

std::string ConvertToPrettyString(Handle<JsValue> value, bool allow_long) {
  const JSValueType type = GetValueType(value);

  switch (type) {
    case JSValueType::Undefined:
    case JSValueType::Null:
    case JSValueType::Boolean:
    case JSValueType::Number:
      return ConvertToString(value);

    case JSValueType::Function:
      return "function() {...}";

    case JSValueType::String: {
      const std::string string = ConvertToString(value);
      return ConvertStringToPrettyString(string);
    }
    case JSValueType::Array:
      return allow_long ? ConvertArrayToLongPrettyString(value) : "[...]";

    case JSValueType::Symbol:
      return "Symbol(" + ConvertToString(value) + ")";
    case JSValueType::BooleanObject:
      return "Boolean(" + ConvertToString(value) + ")";
    case JSValueType::NumberObject:
      return "Number(" + ConvertToString(value) + ")";
    case JSValueType::StringObject:
      return "String(" + ConvertStringToPrettyString(ConvertToString(value)) +
             ")";

    default:
      if (!IsObject(value))
        return ConvertToString(value);

      LocalVar<JsObject> object = UnsafeJsCast<JsObject>(value);
      if (IsBuiltInObject(object))
        return ConvertToString(value);
      return allow_long ? ConvertObjectToLongPrettyString(object) : "{...}";
  }
}

}  // namespace

Console::Console() {}
// \cond Doxygen_Skip
Console::~Console() {}
// \endcond Doxygen_Skip

void Console::Assert(Any cond, const CallbackArguments& arguments) const {
  if (!cond.IsTruthy()) {
    LogReal(kError, arguments, "Assertion failed: ", 1);
    std::printf("%s\n", JsError::GetJsStack().c_str());
  }
}

void Console::Error(const CallbackArguments& arguments) const {
  LogReal(kError, arguments);
}

void Console::Warn(const CallbackArguments& arguments) const {
  LogReal(kWarn, arguments);
}

void Console::Info(const CallbackArguments& arguments) const {
  LogReal(kInfo, arguments);
}

void Console::Log(const CallbackArguments& arguments) const {
  LogReal(kLog, arguments);
}

void Console::Debug(const CallbackArguments& arguments) const {
  LogReal(kDebug, arguments);
}

std::string Console::ConvertToPrettyString(Handle<JsValue> value) {
  return shaka::js::ConvertToPrettyString(value, true);
}

void Console::LogReal(LogLevel level, const CallbackArguments& arguments,
                      const char* prefix, size_t skip_count) const {
  // TODO: Consider using glog for logging.
  std::printf("[%s]: ", to_string(level).c_str());
  if (prefix)
    std::printf("%s", prefix);
  const size_t length = ArgumentCount(arguments);
  for (size_t i = skip_count; i < length; ++i) {
    if (i > skip_count)
      std::printf("\t");
    std::printf("%s", ConvertToPrettyString(arguments[i]).c_str());
  }
  std::printf("\n");
}

ConsoleFactory::ConsoleFactory() {
  AddMemberFunction("assert", &Console::Assert);
  AddMemberFunction("error", &Console::Error);
  AddMemberFunction("warn", &Console::Warn);
  AddMemberFunction("info", &Console::Info);
  AddMemberFunction("log", &Console::Log);
  AddMemberFunction("debug", &Console::Debug);
}

}  // namespace js
}  // namespace shaka
