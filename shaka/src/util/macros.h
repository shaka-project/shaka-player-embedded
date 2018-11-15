// Copyright 2017 Google LLC
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

#ifndef SHAKA_EMBEDDED_UTIL_MACROS_H_
#define SHAKA_EMBEDDED_UTIL_MACROS_H_

#include <glog/logging.h>

#include <iostream>
#include <string>

#define NON_COPYABLE_TYPE(Type) \
  Type(const Type&) = delete;   \
  Type& operator=(const Type&) = delete

#define NON_MOVABLE_TYPE(Type) \
  Type(Type&&) = delete;       \
  Type& operator=(Type&&) = delete

#define NON_COPYABLE_OR_MOVABLE_TYPE(Type) \
  NON_COPYABLE_TYPE(Type);                 \
  NON_MOVABLE_TYPE(Type)

#define LOG_ONCE(severity) LOG_FIRST_N(severity, 1)


#ifdef __GNUC__
#  define PRINTF_FORMAT(format_arg, dots_arg) \
    __attribute__((format(printf, format_arg, dots_arg)))
#  define NO_SANITIZE(name) __attribute__((no_sanitize(name)))
#  define FALL_THROUGH_INTENDED [[clang::fallthrough]]  // NOLINT
#  define BEGIN_ALLOW_COMPLEX_STATICS                                   \
    _Pragma("clang diagnostic push")                                    \
        _Pragma("clang diagnostic ignored \"-Wexit-time-destructors\"") \
            _Pragma("clang diagnostic ignored \"-Wglobal-constructors\"")
#  define END_ALLOW_COMPLEX_STATICS _Pragma("clang diagnostic pop")
#else
#  define PRINTF_FORMAT(format, dots)
#  define NO_SANITIZE(name)
#  define FALL_THROUGH_INTENDED
#  define BEGIN_ALLOW_COMPLEX_STATICS
#  define END_ALLOW_COMPLEX_STATICS
#endif

#ifdef __GNUC__
#  define MUST_USE_RESULT __attribute__((warn_unused_result))
#elif defined(_MSC_VER) && _MSC_VER >= 1700
#  include <sal.h>
#  define MUST_USE_RESULT _Check_return_
#else
#  define MUST_USE_RESULT
#endif

// Type is the name of the generated type.
// DEFINE_ENUM is a macro that will define the enum; it will be called twice
//   given either ENUM_IMPL or CASE_IMPL.
// ENUM_IMPL and CASE_IMPL are macros that are passed as arguments to
//   DEFINE_ENUM.
#define DEFINE_ENUM_AND_TO_STRING_GENERIC_(Type, DEFINE_ENUM, ENUM_IMPL, \
                                           CASE_IMPL)                    \
  enum class Type { DEFINE_ENUM(ENUM_IMPL) };                            \
  inline std::string to_string(Type e) noexcept {                        \
    using Enum = Type;                                                   \
    switch (e) {                                                         \
      DEFINE_ENUM(CASE_IMPL)                                             \
      default:                                                           \
        LOG(FATAL) << "Unknown enum value " << static_cast<int>(e);      \
    }                                                                    \
  }                                                                      \
  inline std::ostream& operator<<(std::ostream& os, Type e) {            \
    return os << to_string(e);                                           \
  }

#define DEFINE_ENUM_IMPL_(name) name,
#define DEFINE_CASE_IMPL_(name) case Enum::name: return #name;
/**
 * Defines an enum type and a to_string method.  This should be given the name
 * of the type to define and a macro that defines the enum.  The macro will be
 * given another macro that should be called repeatedly with the enum to define.
 *
 * Example:
 * \code
 *   #define OUR_DEFINE_ENUM(DEFINE) \
 *     DEFINE(EnumValue)             \
 *     DEFINE(OtherValue)
 * \endcode
 */
#define DEFINE_ENUM_AND_TO_STRING(Type, DEFINE_ENUM)                       \
  DEFINE_ENUM_AND_TO_STRING_GENERIC_(Type, DEFINE_ENUM, DEFINE_ENUM_IMPL_, \
                                     DEFINE_CASE_IMPL_)

#define DEFINE_ENUM_IMPL_2_(name, str) name,
#define DEFINE_CASE_IMPL_2_(name, str) case Enum::name: return str;
/**
 * @see DEFINE_ENUM_AND_TO_STRING
 * This is the same except that the macro given to DEFINE_ENUM should be called
 * with two arguments: the enum name and the string value.
 *
 * Example:
 * \code
 *   #define OUR_DEFINE_ENUM(DEFINE) \
 *     DEFINE(EnumValue, "enum")     \
 *     DEFINE(OtherValue, "other")
 * \endcode
 */
#define DEFINE_ENUM_AND_TO_STRING_2(Type, DEFINE_ENUM)                       \
  DEFINE_ENUM_AND_TO_STRING_GENERIC_(Type, DEFINE_ENUM, DEFINE_ENUM_IMPL_2_, \
                                     DEFINE_CASE_IMPL_2_)

#endif  // SHAKA_EMBEDDED_UTIL_MACROS_H_
