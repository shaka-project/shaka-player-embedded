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

#ifndef SHAKA_EMBEDDED_OPTIONAL_H_
#define SHAKA_EMBEDDED_OPTIONAL_H_

#include <assert.h>

#include <type_traits>
#include <utility>

namespace shaka {

// Note that we shouldn't use the C++17 type even if we are compiling with that
// version of C++.  This is because Shaka Embedded is compiled using C++11 and
// will use this type.  So the app should always use this type for our API.
// Otherwise using different types will cause subtle bugs.

/** @see https://en.cppreference.com/w/cpp/utility/optional/nullopt_t */
struct nullopt_t {
  explicit nullopt_t(int) {}
};
extern const nullopt_t nullopt;

template <class T>
class optional;

template <class T>
struct is_optional : std::false_type {};
template <class T>
struct is_optional<optional<T>> : std::true_type {};

/**
 * This is a look-alike for the std::optional type.  The most common
 * usages of this type have been implemented, but this is not a general
 * implementation.  For example, this doesn't throw exceptions and instead
 * asserts.
 *
 * @see https://en.cppreference.com/w/cpp/utility/optional
 */
template <class T>
class optional {
 public:
  optional() : has_value_(false) {}
  optional(nullopt_t) : has_value_(false) {}
  // Avoid errors when returning |nullptr| instead of |nullopt|.
  optional(std::nullptr_t) = delete;
  template <class U = T,
            class = typename std::enable_if<
                std::is_constructible<T, U&&>::value &&
                !is_optional<typename std::decay<U>::type>::value>::type>
  optional(U&& value) : value_(std::forward<U>(value)), has_value_(true) {}

  optional(const optional& other) : has_value_(other.has_value_) {
    if (has_value_)
      new (&value_) T(other.value_);
  }
  optional(optional&& other) : has_value_(other.has_value_) {
    if (has_value_) {
      new (&value_) T(std::move(other.value_));
      other.reset();
    }
  }
  template <class U>
  optional(const optional<U>& other) : has_value_(other.has_value_) {
    if (has_value_)
      new (&value_) T(other.value_);
  }
  template <class U>
  optional(optional<U>&& other) : has_value_(other.has_value_) {
    if (has_value_) {
      new (&value_) T(std::move(other.value_));
      other.reset();
    }
  }

  ~optional() {
    reset();
  }

  // This type will still accept assigning to other values but it will use the
  // constructors to create a temporary first.  This avoids having to create a
  // bunch of overloads here.
  optional& operator=(const optional& other) {
    reset();
    has_value_ = other.has_value_;
    if (has_value_)
      new (&value_) T(other.value_);
    return *this;
  }
  optional& operator=(optional&& other) {
    reset();
    has_value_ = other.has_value_;
    if (has_value_) {
      new (&value_) T(std::move(other.value_));
      other.reset();
    }
    return *this;
  }


  const T* operator->() const {
    assert(has_value_);
    return &value_;
  }
  T* operator->() {
    assert(has_value_);
    return &value_;
  }
  const T& operator*() const& {
    assert(has_value_);
    return value_;
  }
  T& operator*() & {
    assert(has_value_);
    return value_;
  }
  const T&& operator*() const&& {
    assert(has_value_);
    return std::move(value_);
  }
  T&& operator*() && {
    assert(has_value_);
    return std::move(value_);
  }

  explicit operator bool() const {
    return has_value_;
  }
  bool has_value() const {
    return has_value_;
  }

  const T& value() const& {
    assert(has_value_);
    return value_;
  }
  T& value() & {
    assert(has_value_);
    return value_;
  }
  const T&& value() const&& {
    assert(has_value_);
    return std::move(value_);
  }
  T&& value() && {
    assert(has_value_);
    return std::move(value_);
  }

  template <class U>
  T value_or(U&& default_value) const& {
    return has_value_ ? value_ : static_cast<T>(std::forward<U>(default_value));
  }
  template <class U>
  T value_or(U&& default_value) && {
    return has_value_ ? std::move(value_)
                      : static_cast<T>(std::forward<U>(default_value));
  }

  void reset() {
    if (has_value_) {
      value_.~T();
      has_value_ = false;
    }
  }

  template <class... Args>
  T& emplace(Args&&... args) {
    reset();
    has_value_ = true;
    new (&value_) T(std::forward<Args>(args)...);
    return value_;
  }

 private:
  template <class U>
  friend class optional;

  union {
    T value_;
    char dummy_;
  };
  bool has_value_;
};
static_assert(sizeof(optional<char>) == 2, "Optional too big");
static_assert(alignof(optional<char>) == alignof(char),
              "Optional bad alignment");


// Note that "no value" < "has value" for any value.
// See https://en.cppreference.com/w/cpp/utility/optional/operator_cmp
template <class A, class B>
bool operator==(const optional<A>& lhs, const optional<B>& rhs) {
  if (!lhs.has_value() || !rhs.has_value())
    return lhs.has_value() == rhs.has_value();
  else
    return lhs.value() == rhs.value();
}
template <class A, class B>
bool operator!=(const optional<A>& lhs, const optional<B>& rhs) {
  return !(lhs == rhs);
}
template <class A, class B>
bool operator<(const optional<A>& lhs, const optional<B>& rhs) {
  if (!lhs.has_value() || !rhs.has_value())
    return rhs.has_value();
  else
    return lhs.value() == rhs.value();
}
template <class A, class B>
bool operator<=(const optional<A>& lhs, const optional<B>& rhs) {
  return lhs < rhs || lhs == rhs;
}
template <class A, class B>
bool operator>(const optional<A>& lhs, const optional<B>& rhs) {
  return rhs < lhs;
}
template <class A, class B>
bool operator>=(const optional<A>& lhs, const optional<B>& rhs) {
  return rhs < lhs || lhs == rhs;
}

template <class T>
bool operator==(const optional<T>& opt, nullopt_t) {
  return !opt;  // 7
}
template <class T>
bool operator==(nullopt_t, const optional<T>& opt) {
  return !opt;  // 8
}
template <class T>
bool operator!=(const optional<T>& opt, nullopt_t) {
  return bool(opt);  // 9
}
template <class T>
bool operator!=(nullopt_t, const optional<T>& opt) {
  return bool(opt);  // 10
}
template <class T>
bool operator<(const optional<T>& opt, nullopt_t) {
  return false;  // 11
}
template <class T>
bool operator<(nullopt_t, const optional<T>& opt) {
  return bool(opt);  // 12
}
template <class T>
bool operator<=(const optional<T>& opt, nullopt_t) {
  return !opt;  // 13
}
template <class T>
bool operator<=(nullopt_t, const optional<T>& opt) {
  return true;  // 14
}
template <class T>
bool operator>(const optional<T>& opt, nullopt_t) {
  return bool(opt);  // 15
}
template <class T>
bool operator>(nullopt_t, const optional<T>& opt) {
  return false;  // 16
}
template <class T>
bool operator>=(const optional<T>& opt, nullopt_t) {
  return true;  // 17
}
template <class T>
bool operator>=(nullopt_t, const optional<T>& opt) {
  return !opt;  // 18
}

template <class A, class B>
bool operator==(const optional<A>& opt, const B& value) {
  return bool(opt) ? *opt == value : false;  // 19
}
template <class A, class B>
bool operator==(const A& value, const optional<B>& opt) {
  return bool(opt) ? value == *opt : false;  // 20
}
template <class A, class B>
bool operator!=(const optional<A>& opt, const B& value) {
  return bool(opt) ? *opt != value : true;  // 21
}
template <class A, class B>
bool operator!=(const A& value, const optional<B>& opt) {
  return bool(opt) ? value == *opt : true;  // 22
}
template <class A, class B>
bool operator<(const optional<A>& opt, const B& value) {
  return bool(opt) ? *opt < value : true;  // 23
}
template <class A, class B>
bool operator<(const A& value, const optional<B>& opt) {
  return bool(opt) ? value < *opt : false;  // 24
}
template <class A, class B>
bool operator<=(const optional<A>& opt, const B& value) {
  return bool(opt) ? *opt <= value : true;  // 25
}
template <class A, class B>
bool operator<=(const A& value, const optional<B>& opt) {
  return bool(opt) ? value <= *opt : false;  // 26
}
template <class A, class B>
bool operator>(const optional<A>& opt, const B& value) {
  return bool(opt) ? *opt > value : false;  // 27
}
template <class A, class B>
bool operator>(const A& value, const optional<B>& opt) {
  return bool(opt) ? value > *opt : true;  // 28
}
template <class A, class B>
bool operator>=(const optional<A>& opt, const B& value) {
  return bool(opt) ? *opt >= value : false;  // 29
}
template <class A, class B>
bool operator>=(const A& value, const optional<B>& opt) {
  return bool(opt) ? value >= *opt : true;  // 30
}

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_OPTIONAL_H_
