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

#include <tuple>
#include <type_traits>

#if defined(USING_V8)
#  include <v8.h>
#endif

namespace shaka {
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


namespace impl {

// \cond Doxygen_Skip
template <typename Func>
struct func_traits : func_traits<decltype(&Func::operator())> {};
// \endcond Doxygen_Skip

template <typename Ret, typename... Args>
struct func_traits<Ret(*)(Args...)> {
  using return_type = Ret;
  using this_type = void;
  using argument_types = std::tuple<Args...>;
  template <size_t I>
  using argument_type = typename std::tuple_element<I, argument_types>::type;

  static constexpr const size_t argument_count = sizeof...(Args);
};

template <typename Ret, typename This, typename... Args>
struct func_traits<Ret(This::*)(Args...)> {
  using return_type = Ret;
  using this_type = This;
  using argument_types = std::tuple<Args...>;
  template <size_t I>
  using argument_type = typename std::tuple_element<I, argument_types>::type;

  static constexpr const size_t argument_count = sizeof...(Args);
};

template <typename Ret, typename This, typename... Args>
struct func_traits<Ret(This::*)(Args...) const> {
  using return_type = Ret;
  using this_type = This;
  using argument_types = std::tuple<Args...>;
  template <size_t I>
  using argument_type = typename std::tuple_element<I, argument_types>::type;

  static constexpr const size_t argument_count = sizeof...(Args);
};

}  // namespace impl

/**
 * Describes info about a function object.  This assumes the function object
 * only has one overload of the call operator and the call operator is not
 * templated.
 */
template <typename Func>
struct func_traits : impl::func_traits<typename std::decay<Func>::type> {
  using type = Func;
};

}  // namespace util
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTIL_TEMPLATES_H_
