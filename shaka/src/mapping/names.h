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

#ifndef SHAKA_EMBEDDED_MAPPING_NAMES_H_
#define SHAKA_EMBEDDED_MAPPING_NAMES_H_

#include <string>
#include <type_traits>
#include <vector>

#include "shaka/optional.h"
#include "src/util/templates.h"

namespace shaka {

class BackingObject;

/**
 * Contains a single method |name| that returns a string for the name of the
 * type. For custom types, it requires a static method |name| to exist on
 * the type T. This contains overrides for built-in types.
 *
 * The second template parameter should always be void.  This is to allow
 * partial specialization for numbers.  When instantiating a template, the
 * compiler first gets the default as specified in the base definition.  Then
 * it checks each specialization to see if it can infer arguments that will
 * match that specialization.
 *
 * So when passing (int), it will expand to (int, void), then check each
 * specialization.  When it gets to the one for numbers, it will check the
 * enable_if_t; the enable_if_t will result in void if the first type is a
 * number, or an error if not.  This will cause that specialization to be
 * used if T is a number and will be ignored if it is not a number.
 */
template <typename T, typename = void>
struct TypeName {
  static std::string name() {
    return T::name();
  }
};

template <typename T>
struct TypeName<shaka::optional<T>> {
  static std::string name() {
    return "optional " + TypeName<T>::name();
  }
};

template <typename... Types>
struct TypeName<shaka::variant<Types...>> {
 private:
  template <size_t I, typename = void>
  struct Helper {
    using T = variant_alternative_t<I, shaka::variant<Types...>>;
    static std::string name() {
      return TypeName<T>::name() + " or " + Helper<I + 1>::name();
    }
  };
  template <typename Dummy>
  struct Helper<sizeof...(Types) - 1, Dummy> {
    using T =
        variant_alternative_t<sizeof...(Types) - 1, shaka::variant<Types...>>;
    static std::string name() {
      return TypeName<T>::name();
    }
  };

 public:
  static std::string name() {
    return Helper<0>::name();
  }
};

template <typename T>
struct TypeName<std::vector<T>, void> {
  static std::string name() {
    return "array of " + TypeName<T>::name();
  }
};

template <typename T>
struct TypeName<T*, void> : TypeName<T> {};

template <typename T>
struct TypeName<T, util::enable_if_t<util::is_number<T>::value>> {
  static std::string name() {
    return "number";
  }
};

template <>
struct TypeName<BackingObject, void> {
  static std::string name() {
    return "BackingObject";
  }
};
template <>
struct TypeName<bool, void> {
  static std::string name() {
    return "boolean";
  }
};
template <>
struct TypeName<std::string, void> {
  static std::string name() {
    return "string";
  }
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_NAMES_H_
