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

#ifndef SHAKA_EMBEDDED_UTIL_TEMPLATES_H_
#define SHAKA_EMBEDDED_UTIL_TEMPLATES_H_

#include <type_traits>

#if defined(USING_V8)
#  include <v8.h>
#endif

namespace shaka {

template <typename T>
class optional;

namespace util {

/**
 * A conditional type to determine if a type T is a V8 type.  Note that for
 * incomplete types, this returns false as we never forward-declare V8 types.
 */
template <typename T, typename = void>
struct is_v8_type : std::false_type {};
// Can only call sizeof(T) for complete types.
#if defined(USING_V8)
template <typename T>
struct is_v8_type<T, decltype(void(sizeof(T)))> : std::is_base_of<v8::Data, T> {
};
#endif

/**
 * A conditional type to determine whether the given type is a JavaScript
 * number, namely a number (non-bool) type or an enumeration.
 */
template <typename T>
struct is_number
    : std::integral_constant<bool, std::is_arithmetic<T>::value &&
                                       !std::is_same<T, bool>::value> {};

template <typename A, typename B>
struct decay_is_same
    : std::is_same<typename std::decay<A>::type, typename std::decay<B>::type> {
};

template <bool B, typename T = void>
using enable_if_t = typename std::enable_if<B, T>::type;

}  // namespace util
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTIL_TEMPLATES_H_
