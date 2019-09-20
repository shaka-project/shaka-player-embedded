// Copyright 2018 Google LLC
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

#ifndef SHAKA_EMBEDDED_VARIANT_H_
#define SHAKA_EMBEDDED_VARIANT_H_

#include <assert.h>
#include <stddef.h>

#include <type_traits>
#include <utility>

namespace shaka {

// Note that we shouldn't use the C++17 type even if we are compiling with that
// version of C++.  This is because Shaka Embedded is compiled using C++11 and
// will use this type.  So the app should always use this type for our API.
// Otherwise using different types will cause subtle bugs.

template <typename... Choices>
class variant;

namespace impl {

/** Contains a |type| member for the type at the given index in the pack. */
template <size_t I, typename... Choices>
struct get_type_at;
template <size_t I, typename T, typename... Choices>
struct get_type_at<I, T, Choices...> final {
  static_assert(I > 0, "Bad specialization chosen.");
  using type = typename get_type_at<I - 1, Choices...>::type;
};
template <typename T, typename... Choices>
struct get_type_at<0, T, Choices...> final {
  using type = T;
};

template <size_t I, typename... Choices>
using get_type_at_t = typename get_type_at<I, Choices...>::type;

template <size_t I, typename... Choices>
using get_const_reference_at_t =
    typename std::add_lvalue_reference<typename std::add_const<
        typename get_type_at<I, Choices...>::type>::type>::type;

/** Contains an |index| member for the index of the given type in the pack. */
template <typename T, typename... Choices>
struct get_type_index;
template <typename T, typename... Choices>
struct get_type_index<T, T, Choices...> {
  static constexpr const size_t index = 0;
};
template <typename T, typename A, typename... Choices>
struct get_type_index<T, A, Choices...> {
  static_assert(!std::is_same<T, A>::value, "Bad specialization chosen");
  static constexpr const size_t index =
      get_type_index<T, Choices...>::index + 1;
};

/**
 * Contains an |index| member for the index of the first type that can be
 * constructed with an argument of type Arg.
 */
template <typename Arg, typename T, typename... Choices>
struct get_construct_index {
 private:
  // We can't use a ternary conditional to terminate the recursion, so we have
  // to use a specialization like this.
  template <bool CanConstruct, typename Dummy = void>
  struct Helper {  // CanConstruct = true;
    static constexpr const size_t index = 0;
  };
  template <typename Dummy>
  struct Helper<false, Dummy> {
    static constexpr const size_t index =
        get_construct_index<Arg, Choices...>::index + 1;
  };

 public:
  static constexpr const size_t index =
      Helper<std::is_constructible<T, Arg&&>::value>::index;
};

/**
 * Represents a union of the given types.  This doesn't track which type is
 * contained and assumes the caller will.  The caller needs to explicitly call
 * emplace() and reset() to set and clear the value respectively.
 */
template <typename T, typename... Rest>
class union_ final {
 public:
  union_() {}
  union_(const union_&) = delete;
  union_(union_&&) = delete;
  ~union_() {}

  union_& operator=(const union_&) = delete;
  union_& operator=(union_&&) = delete;

  void copy(const union_& other, size_t i) {
    if (i == 0) {
      new (&value_) T(other.value_);
    } else {
      new (&rest_) union_<Rest...>;
      rest_.copy(other.rest_, i - 1);
    }
  }

  void move(union_&& other, size_t i) {
    if (i == 0) {
      new (&value_) T(std::move(other.value_));
    } else {
      new (&rest_) union_<Rest...>;
      rest_.move(std::move(other.rest_), i - 1);
    }
  }

  template <size_t I>
  get_const_reference_at_t<I, T, Rest...> get() const {
    return GetHelper<I, void*>::get(this);
  }

  template <size_t I>
  get_type_at_t<I, T, Rest...>& get() {
    return GetHelper<I, void*>::get(this);
  }

  // Note this works for copy- and move-constructors too.
  template <size_t I, typename... U>
  void emplace(U&&... args) {
    EmplaceHelper<I, U...>::emplace(this, std::forward<U>(args)...);
  }

  void reset(size_t i) {
    if (i == 0) {
      value_.~T();
    } else {
      rest_.reset(i - 1);
      rest_.~union_<Rest...>();
    }
  }

  bool equals(const union_& other, size_t i) const {
    if (i == 0)
      return value_ == other.value_;
    else
      return rest_.equals(other.rest_, i - 1);
  }

 private:
  template <size_t I, typename... U>
  struct EmplaceHelper {
    static void emplace(union_* that, U&&... args) {
      new (&that->rest_) union_<Rest...>;
      that->rest_.template emplace<I - 1>(std::forward<U>(args)...);
    }
  };
  template <typename... U>
  struct EmplaceHelper<0, U...> {
    static void emplace(union_* that, U&&... args) {
      new (&that->value_) T(std::forward<U>(args)...);
    }
  };

  template <size_t I, typename Dummy = void>
  struct GetHelper {
    static get_const_reference_at_t<I, T, Rest...> get(const union_* that) {
      return that->rest_.template get<I - 1>();
    }

    static get_type_at_t<I, T, Rest...>& get(union_* that) {
      return that->rest_.template get<I - 1>();
    }
  };
  template <typename Dummy>
  struct GetHelper<0, Dummy> {
    static const T& get(const union_* that) {
      return that->value_;
    }

    static T& get(union_* that) {
      return that->value_;
    }
  };

  union {
    T value_;
    union_<Rest...> rest_;
  };
};

template <typename T>
class union_<T> final {
 public:
  union_() {}
  union_(const union_&) = delete;
  union_(union_&&) = delete;
  ~union_() {}

  union_& operator=(const union_&) = delete;
  union_& operator=(union_&&) = delete;

  void copy(const union_& other, size_t i) {
    assert(i == 0);
    new (&value_) T(other.value_);
  }

  void move(union_&& other, size_t i) {
    assert(i == 0);
    new (&value_) T(std::move(other.value_));
  }

  template <size_t I>
  const T& get() const {
    static_assert(I == 0, "Index out of range");
    return value_;
  }

  template <size_t I>
  T& get() {
    static_assert(I == 0, "Index out of range");
    return value_;
  }

  // Note this works for copy- and move-constructors too.
  template <size_t I, typename... U>
  void emplace(U&&... args) {
    static_assert(I == 0, "Index out of range");
    new (&value_) T(std::forward<U>(args)...);
  }

  void reset(size_t i) {
    assert(i == 0);
    value_.~T();
  }

  bool equals(const union_& other, size_t i) const {
    assert(i == 0);
    return value_ == other.value_;
  }

 private:
  union {
    T value_;
    void* dummy_;
  };
};

template <typename T>
struct is_variant : std::false_type {};
template <typename... Choices>
struct is_variant<variant<Choices...>> : std::true_type {};

}  // namespace impl

template <size_t I, typename Variant>
struct variant_alternative;
template <size_t I, typename... Choices>
struct variant_alternative<I, variant<Choices...>> {
  using type = typename impl::get_type_at<I, Choices...>::type;
};

template <size_t I, typename Variant>
using variant_alternative_t = typename variant_alternative<I, Variant>::type;

struct monostate {};

/**
 * This is a look-alike for the std::variant type.  The most common
 * usages of this type have been implemented, but this is not a general
 * implementation.  For example, this doesn't throw exceptions and instead
 * asserts.
 *
 * @see https://en.cppreference.com/w/cpp/utility/variant
 */
template <typename... Types>
class variant {
 public:
  variant() : index_(0) {
    union_.template emplace<0>();
  }

  template <typename U, typename = typename std::enable_if<!impl::is_variant<
                            typename std::decay<U>::type>::value>::type>
  variant(U&& value) {
    constexpr const size_t I = impl::get_construct_index<U, Types...>::index;
    union_.template emplace<I>(std::forward<U>(value));
    index_ = I;
  }

  variant(const variant& other) : index_(other.index_) {
    union_.copy(other.union_, other.index_);
  }

  variant(variant&& other) : index_(other.index_) {
    union_.move(std::move(other.union_), other.index_);
  }

  ~variant() {
    union_.reset(index_);
  }

  variant& operator=(const variant& other) {
    union_.reset(index_);
    union_.copy(other.union_, other.index_);
    index_ = other.index_;
    return *this;
  }
  variant& operator=(variant&& other) {
    union_.reset(index_);
    union_.move(std::move(other.union_), other.index_);
    index_ = other.index_;
    return *this;
  }

  size_t index() const {
    return index_;
  }

  template <typename T, typename... Args>
  T& emplace(Args&&... args) {
    constexpr const size_t I = impl::get_type_index<T, Types...>::index;
    union_.reset(index_);
    union_.template emplace<I>(std::forward<Args>(args)...);
    index_ = I;
    return union_.template get<I>();
  }

  template <size_t I, typename... Args>
  variant_alternative_t<I, variant>& emplace(Args&&... args) {
    union_.reset(index_);
    union_.template emplace<I>(std::forward<Args>(args)...);
    index_ = I;
    return union_.template get<I>();
  }

 private:
  template <typename...>
  friend class variant;
  template <typename... Choices>
  friend bool operator==(const variant<Choices...>&,
                         const variant<Choices...>&);

  template <size_t I, typename... Choices>
  friend impl::get_const_reference_at_t<I, Choices...> get(
      const variant<Choices...>&);
  template <size_t I, typename... Choices>
  friend variant_alternative_t<I, variant<Choices...>>& get(
      variant<Choices...>&);
  template <typename T, typename... Choices>
  friend const T& get(const variant<Choices...>&);
  template <typename T, typename... Choices>
  friend T& get(variant<Choices...>&);
  template <typename T, typename... Choices>
  friend T&& get(variant<Choices...>&&);

  template <size_t I, typename... Choices>
  friend typename std::add_const<
      variant_alternative_t<I, variant<Choices...>>>::type*
  get_if(const variant<Choices...>&);
  template <size_t I, typename... Choices>
  friend variant_alternative_t<I, variant<Choices...>>* get_if(
      variant<Choices...>&);
  template <typename T, typename... Choices>
  friend const T* get_if(const variant<Choices...>&);
  template <typename T, typename... Choices>
  friend T* get_if(variant<Choices...>&);


  bool equals(const variant& other) const {
    return index_ == other.index_ && union_.equals(other.union_, index_);
  }

  template <size_t I>
  impl::get_const_reference_at_t<I, Types...> get() const {
    assert(I == index_);
    return union_.template get<I>();
  }

  template <size_t I>
  variant_alternative_t<I, variant>& get() {
    assert(I == index_);
    return union_.template get<I>();
  }

  template <size_t I>
  variant_alternative_t<I, variant>&& get() && {
    assert(I == index_);
    return std::move(union_.template get<I>());
  }

  impl::union_<Types...> union_;
  size_t index_;
};


template <typename... Choices>
bool operator==(const variant<Choices...>& lhs,
                const variant<Choices...>& rhs) {
  return lhs.equals(rhs);
}

template <typename... Choices>
bool operator!=(const variant<Choices...>& lhs,
                const variant<Choices...>& rhs) {
  return !(lhs == rhs);
}


template <typename T, typename... Choices>
bool holds_alternative(const variant<Choices...>& variant) {
  constexpr const size_t I = impl::get_type_index<T, Choices...>::index;
  return variant.index() == I;
}


template <size_t I, typename... Choices>
impl::get_const_reference_at_t<I, Choices...> get(
    const variant<Choices...>& variant) {
  return variant.template get<I>();
}

template <size_t I, typename... Choices>
variant_alternative_t<I, variant<Choices...>>& get(
    variant<Choices...>& variant) {
  return variant.template get<I>();
}

template <typename T, typename... Choices>
const T& get(const variant<Choices...>& variant) {
  constexpr const size_t I = impl::get_type_index<T, Choices...>::index;
  return variant.template get<I>();
}

template <typename T, typename... Choices>
T& get(variant<Choices...>& variant) {
  constexpr const size_t I = impl::get_type_index<T, Choices...>::index;
  return variant.template get<I>();
}

template <typename T, typename... Choices>
T&& get(variant<Choices...>&& variant) {
  constexpr const size_t I = impl::get_type_index<T, Choices...>::index;
  return std::move(variant.template get<I>());
}


template <size_t I, typename... Choices>
typename std::add_const<variant_alternative_t<I, variant<Choices...>>>::type*
get_if(const variant<Choices...>& variant) {
  if (variant.index() == I)
    return &variant.template get<I>();
  else
    return nullptr;
}

template <size_t I, typename... Choices>
variant_alternative_t<I, variant<Choices...>>* get_if(
    variant<Choices...>& variant) {
  if (variant.index() == I)
    return &variant.template get<I>();
  else
    return nullptr;
}

template <typename T, typename... Choices>
const T* get_if(const variant<Choices...>& variant) {
  constexpr const size_t I = impl::get_type_index<T, Choices...>::index;
  if (variant.index() == I)
    return &variant.template get<I>();
  else
    return nullptr;
}

template <typename T, typename... Choices>
T* get_if(variant<Choices...>& variant) {
  constexpr const size_t I = impl::get_type_index<T, Choices...>::index;
  if (variant.index() == I)
    return &variant.template get<I>();
  else
    return nullptr;
}

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_VARIANT_H_
