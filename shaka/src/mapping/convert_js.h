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

#ifndef SHAKA_EMBEDDED_MAPPING_CONVERT_JS_H_
#define SHAKA_EMBEDDED_MAPPING_CONVERT_JS_H_

#include <cmath>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "shaka/optional.h"
#include "shaka/variant.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/generic_converter.h"
#include "src/mapping/js_wrappers.h"
#include "src/util/templates.h"

namespace shaka {

namespace impl {

// Types used to identify a category of types.  These allow a specialization of
// ConvertHelper to be used on a set of types (for example, numbers).
struct _number_identifier {};
struct _generic_converter_identifier {};
struct _callable_identifier {};

template <typename T, typename = void>
struct _SelectSpecialTypes {
  using type = void;
};
#define ADD_SPECIAL_TYPE(id, ...)                                 \
  template <typename T>                                           \
  struct _SelectSpecialTypes<T, util::enable_if_t<__VA_ARGS__>> { \
    using type = id;                                              \
  }

ADD_SPECIAL_TYPE(_number_identifier, util::is_number<T>::value);
ADD_SPECIAL_TYPE(_generic_converter_identifier,
                 std::is_base_of<GenericConverter, T>::value);
ADD_SPECIAL_TYPE(_callable_identifier, util::is_callable<T>::value);
#undef ADD_SPECIAL_TYPE

/**
 * A template type that defines two conversion functions to convert between C++
 * and JavaScript.  This should be instantiated with a single type argument for
 * the type being converted (without cv-qualifiers or references).  The second
 * template argument is deduced based on the first.  It is used to select
 * which specialization to use.
 *
 * It is fine to add more specializations after this, so long as it appears
 * before it is being used.  If the specialization is for a specific type
 * (e.g. MyType), or it is for a templated type (e.g. std::vector<T>), you
 * can just add a new specialization:
 *
 * \code
 *   template <> struct ConvertHelper<MyType, void> { ... };
 *   // -or-
 *   template <typename T> struct ConvertHelper<std::vector<T>, void> { ... };
 * \endcode
 *
 * If the specialization is for a "category" of types (e.g. numbers, where the
 * type would be T), then you need to add a new identifier and specialization
 * above.
 */
template <typename T, typename = typename _SelectSpecialTypes<T>::type>
struct ConvertHelper;

template <typename Number>
struct ConvertHelper<Number, _number_identifier> {
  static bool FromJsValue(Handle<JsValue> source, Number* dest) {
    // Number types.
    const proto::ValueType type = GetValueType(source);
    if (type != proto::ValueType::Number &&
        type != proto::ValueType::NumberObject) {
      return false;
    }
    const double value = NumberFromValue(source);

    if (std::numeric_limits<Number>::has_infinity &&
        std::abs(value) == std::numeric_limits<Number>::infinity()) {
      // OK.
    } else if (value < std::numeric_limits<Number>::lowest() ||
               value > std::numeric_limits<Number>::max()) {
      return false;
    }

    // JavaScript numbers will be intentionally truncated if stored as native
    // integers.
    *dest = static_cast<Number>(value);
    return true;
  }

  static ReturnVal<JsValue> ToJsValue(Number source) {
#if defined(USING_V8)
    return v8::Number::New(GetIsolate(), source);
#elif defined(USING_JSC)
    return JSValueMakeNumber(GetContext(), source);
#endif
  }
};

template <typename T>
struct ConvertHelper<T, _generic_converter_identifier> {
  static bool FromJsValue(Handle<JsValue> source, T* dest) {
    return dest->TryConvert(source);
  }

  static ReturnVal<JsValue> ToJsValue(const T& source) {
    return source.ToJsValue();
  }
};

template <typename T>
struct ConvertHelper<shaka::optional<T>, void> {
  static bool FromJsValue(Handle<JsValue> source, shaka::optional<T>* dest) {
    if (IsNullOrUndefined(source)) {
      dest->reset();
      return true;
    }

    T temp;
    if (!ConvertHelper<T>::FromJsValue(source, &temp))
      return false;
    *dest = std::move(temp);
    return true;
  }

  static ReturnVal<JsValue> ToJsValue(const shaka::optional<T>& source) {
    if (source.has_value()) {
      return ConvertHelper<T>::ToJsValue(source.value());
    } else {
      return JsNull();
    }
  }
};

template <typename... Types>
struct ConvertHelper<shaka::variant<Types...>> {
 private:
  template <size_t I, typename = void>
  struct Helper {
    using T = variant_alternative_t<I, shaka::variant<Types...>>;

    static bool FromJsValue(Handle<JsValue> source,
                            shaka::variant<Types...>* dest) {
      T temp;
      if (ConvertHelper<T>::FromJsValue(source, &temp)) {
        dest->template emplace<I>(std::move(temp));
        return true;
      }
      return Helper<I + 1>::FromJsValue(source, dest);
    }

    static ReturnVal<JsValue> ToJsValue(
        const shaka::variant<Types...>& source) {
      if (source.index() == I)
        return ConvertHelper<T>::ToJsValue(get<I>(source));
      else
        return Helper<I + 1>::ToJsValue(source);
    }
  };
  template <typename Dummy>
  struct Helper<sizeof...(Types) - 1, Dummy> {
    static constexpr const size_t I = sizeof...(Types) - 1;
    using T = variant_alternative_t<I, shaka::variant<Types...>>;

    static bool FromJsValue(Handle<JsValue> source,
                            shaka::variant<Types...>* dest) {
      T temp;
      if (ConvertHelper<T>::FromJsValue(source, &temp)) {
        dest->template emplace<I>(std::move(temp));
        return true;
      }
      return false;
    }

    static ReturnVal<JsValue> ToJsValue(
        const shaka::variant<Types...>& source) {
      CHECK_EQ(source.index(), I);
      return ConvertHelper<T>::ToJsValue(get<I>(source));
    }
  };

 public:
  static bool FromJsValue(Handle<JsValue> source,
                          shaka::variant<Types...>* dest) {
    return Helper<0>::FromJsValue(source, dest);
  }

  static ReturnVal<JsValue> ToJsValue(const shaka::variant<Types...>& source) {
    return Helper<0>::ToJsValue(source);
  }
};

template <typename T>
struct ConvertHelper<std::vector<T>, void> {
  static bool FromJsValue(Handle<JsValue> source, std::vector<T>* dest) {
    if (GetValueType(source) != proto::ValueType::Array)
      return false;

    LocalVar<JsObject> array(UnsafeJsCast<JsObject>(source));
    const size_t length = ArrayLength(array);
    // Store the data in a temp array since this should not modify |*dest| on
    // failure.
    std::vector<T> temp(length);
    DCHECK_EQ(length, temp.size());
    for (size_t i = 0; i < length; i++) {
      LocalVar<JsValue> item(GetArrayIndexRaw(array, i));
      if (!ConvertHelper<T>::FromJsValue(item, &temp[i]))
        return false;
    }

    swap(temp, *dest);
    return true;
  }

  static ReturnVal<JsValue> ToJsValue(const std::vector<T>& source) {
    LocalVar<JsObject> ret(CreateArray(source.size()));
    for (size_t i = 0; i < source.size(); i++) {
      LocalVar<JsValue> item(ConvertHelper<T>::ToJsValue(source[i]));
      SetArrayIndexRaw(ret, i, item);
    }
    return RawToJsValue(ret);
  }
};

template <typename Key, typename Value>
struct ConvertHelper<std::unordered_map<Key, Value>, void> {
  static bool FromJsValue(Handle<JsValue> source,
                          std::unordered_map<Key, Value>* dest) {
    static_assert(
        std::is_same<typename std::decay<Key>::type, std::string>::value,
        "Can only use std::string as a key for maps");
    if (!IsObject(source))
      return false;

    LocalVar<JsObject> map(UnsafeJsCast<JsObject>(source));
    if (IsBuiltInObject(map))
      return false;

    // Store the data in a temp array since this should not modify |*dest| on
    // failure.
    std::unordered_map<Key, Value> temp;
    for (const auto& name : GetMemberNames(map)) {
      LocalVar<JsValue> item(GetMemberRaw(map, name));

      Value temp_field;
      if (!ConvertHelper<Value>::FromJsValue(item, &temp_field))
        return false;
      temp.emplace(name, std::move(temp_field));
    }

    swap(temp, *dest);
    return true;
  }

  static ReturnVal<JsValue> ToJsValue(
      const std::unordered_map<Key, Value>& source) {
    LocalVar<JsObject> ret(CreateObject());
    for (auto& pair : source) {
      LocalVar<JsValue> value(ConvertHelper<Value>::ToJsValue(pair.second));
      SetMemberRaw(ret, pair.first, value);
    }
    return RawToJsValue(ret);
  }
};

template <typename T>
struct ConvertHelper<T*, void> {
  static_assert(std::is_base_of<BackingObject, T>::value,
                "Don't accept raw pointers as arguments, use RefPtr<T>.");

  static ReturnVal<JsValue> ToJsValue(T* source) {
    // We cannot implicitly convert a T* to a RefPtr<T> because the compiler
    // cannot deduce the type parameter.  This allows us to pass raw pointers
    // (e.g. |this|).
    if (!source)
      return JsNull();
    else
      return source->JsThis();
  }
};

template <typename T>
struct ConvertHelper<ReturnVal<T>, void> {
  static ReturnVal<JsValue> ToJsValue(const ReturnVal<T>& source) {
    return RawToJsValue(source);
  }
};

template <>
struct ConvertHelper<Handle<JsObject>, void> {
  static bool FromJsValue(Handle<JsValue> source, Handle<JsObject>* dest) {
    if (!IsObject(source))
      return false;
    *dest = UnsafeJsCast<JsObject>(source);
    return true;
  }
};

template <>
struct ConvertHelper<std::string, void> {
  static bool FromJsValue(Handle<JsValue> source, std::string* dest) {
    const proto::ValueType type = GetValueType(source);
    if (type != proto::ValueType::String &&
        type != proto::ValueType::StringObject) {
      return false;
    }

    *dest = ConvertToString(source);
    return true;
  }

  static ReturnVal<JsValue> ToJsValue(const std::string& source) {
    LocalVar<JsString> str(JsStringFromUtf8(source));
    return RawToJsValue(str);
  }
};

// Note this is only hit for the |bool| type, NOT for things that can be
// implicitly cast to bool, like pointers.
template <>
struct ConvertHelper<bool, void> {
  static bool FromJsValue(Handle<JsValue> source, bool* dest) {
    const proto::ValueType type = GetValueType(source);
    if (type != proto::ValueType::Boolean &&
        type != proto::ValueType::BooleanObject) {
      return false;
    }

    *dest = BooleanFromValue(source);
    return true;
  }

  static ReturnVal<JsValue> ToJsValue(bool source) {
#if defined(USING_V8)
    return v8::Boolean::New(GetIsolate(), source);
#elif defined(USING_JSC)
    return JSValueMakeBoolean(GetContext(), source);
#endif
  }
};

}  // namespace impl

/**
 * Tries to convert the given JavaScript value to the given type.
 * @param source The JavaScript object to convert.
 * @param dest [OUT] Where to put the converted value, not changed if there is
 *   an error.
 * @return True on success, false on error.
 */
template <typename T>
bool FromJsValue(Handle<JsValue> source, T* dest) {
  using BaseT = typename std::decay<T>::type;
  return impl::ConvertHelper<BaseT>::FromJsValue(source, dest);
}

/**
 * Converts the given C++ object to a JavaScript value.
 * @param source The C++ object to convert.
 * @returns A new JavaScript value.
 */
template <typename T>
ReturnVal<JsValue> ToJsValue(T&& source) {
  using BaseT = typename std::decay<T>::type;
  return impl::ConvertHelper<BaseT>::ToJsValue(std::forward<T>(source));
}

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_CONVERT_JS_H_
