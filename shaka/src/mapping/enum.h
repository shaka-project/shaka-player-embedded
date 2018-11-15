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

#ifndef SHAKA_EMBEDDED_MAPPING_ENUM_H_
#define SHAKA_EMBEDDED_MAPPING_ENUM_H_

#include <string>
#include <type_traits>
#include <vector>

#include "src/mapping/convert_js.h"
#include "src/mapping/js_wrappers.h"
#include "src/mapping/names.h"

/**
 * This file defines how to convert C++ enums.  There are two kinds of
 * (conceptual) enums: numbers and strings.  Many browser API standards define
 * JavaScript enums as possible string choices.  The macros below allow defining
 * the mapping between JavaScript values and the C++ enum values. Note that all
 * these macros must be placed at the global namespace.
 *
 * To indicate an enum should just be treated as numbers, simply add the
 * following:
 *
 * \code{cpp}
 * CONVERT_ENUM_AS_NUMBER(shaka::foo, MyEnumType);
 * \endcode
 *
 * To define an enum as string choices, add the following.  Note that the
 * macro defines |Enum| as an alias of the enum type:
 *
 * \code{cpp}
 * DEFINE_ENUM_MAPPING(shaka::foo, MyEnumType) {
 *   AddMapping(Enum::ENUM, "string");
 *   AddMapping(Enum::OTHER, "any-valid-string");
 * }
 * \endcode
 */


#define CONVERT_ENUM_AS_NUMBER(ns, type)        \
  template <>                                   \
  struct ::shaka::TypeName<ns::type, void> {    \
    static std::string name() {                 \
      return #type;                             \
    }                                           \
  };                                            \
  template <>                                   \
  struct ::shaka::impl::ConvertHelper<ns::type> \
      : ::shaka::impl::NumberEnumConverter<ns::type> {}

#define DEFINE_ENUM_MAPPING(ns, type)                                 \
  template <>                                                         \
  struct ::shaka::TypeName<ns::type, void> {                          \
    static std::string name() {                                       \
      return #type;                                                   \
    }                                                                 \
  };                                                                  \
  template <>                                                         \
  class ::shaka::impl::EnumConverter<ns::type>                        \
      : public ::shaka::impl::StringEnumConverter<ns::type> {         \
   public:                                                            \
    using Enum = ns::type;                                            \
    EnumConverter();                                                  \
  };                                                                  \
  template <>                                                         \
  struct ::shaka::impl::ConvertHelper<ns::type> {                     \
    static bool FromJsValue(Handle<JsValue> source, ns::type* dest) { \
      ::shaka::impl::EnumConverter<ns::type> conv;                    \
      return conv.FromJsValue(source, dest);                          \
    }                                                                 \
    static ReturnVal<JsValue> ToJsValue(ns::type source) {            \
      ::shaka::impl::EnumConverter<ns::type> conv;                    \
      return conv.ToJsValue(source);                                  \
    }                                                                 \
  };                                                                  \
  inline ::shaka::impl::EnumConverter<ns::type>::EnumConverter()


namespace shaka {
namespace impl {

template <typename Enum>
class StringEnumConverter {
 public:
  bool FromJsValue(Handle<JsValue> given, Enum* dest) {
    std::string temp;
    if (!ConvertHelper<std::string>::FromJsValue(given, &temp))
      return false;
    for (auto& entry : entries_) {
      if (entry.name == temp) {
        *dest = entry.entry;
        return true;
      }
    }
    return false;
  }

  ReturnVal<JsValue> ToJsValue(Enum value) {
    for (auto& entry : entries_) {
      if (entry.entry == value)
        return ConvertHelper<std::string>::ToJsValue(entry.name);
    }
    LOG(FATAL) << "Invalid enum value.";
  }

 protected:
  void AddMapping(Enum entry, const std::string& name) {
    entries_.emplace_back(entry, name);
  }

 private:
  struct Entry {
    Entry(Enum entry, const std::string& name) : name(name), entry(entry) {}

    std::string name;
    Enum entry;
  };

  std::vector<Entry> entries_;
};

/**
 * Defines a helper converter that treats the |Enum| type as a number.  This
 * does not do bounds checking, namely passing the number 400 from JavaScript
 * when that is not in |Enum| is not an error.
 */
template <typename Enum>
struct NumberEnumConverter {
  static_assert(std::is_enum<Enum>::value, "Must be an enum type.");
  using IntType = typename std::underlying_type<Enum>::type;

  static bool FromJsValue(Handle<JsValue> given, Enum* dest) {
    IntType temp;
    if (!ConvertHelper<IntType>::FromJsValue(given, &temp))
      return false;
    *dest = static_cast<Enum>(temp);
    return true;
  }

  static ReturnVal<JsValue> ToJsValue(Enum value) {
    return ConvertHelper<IntType>::ToJsValue(static_cast<IntType>(value));
  }
};

template <typename T>
class EnumConverter;

}  // namespace impl
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_ENUM_H_
