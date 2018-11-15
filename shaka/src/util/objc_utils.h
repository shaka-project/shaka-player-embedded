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

#ifndef SHAKA_EMBEDDED_UTIL_OBJC_UTILS_H_
#define SHAKA_EMBEDDED_UTIL_OBJC_UTILS_H_

#if !defined(__OBJC__) || !defined(__cplusplus)
#  error "Can only be included from Objective-C++"
#endif

#include <Foundation/Foundation.h>

#include <string>
#include <type_traits>
#include <vector>

#include "shaka/optional.h"

namespace shaka {
namespace util {

template <typename T>
struct ObjcConverter;

template <>
struct ObjcConverter<std::string> {
  static NSString *ToObjc(const std::string &value) {
    return [[NSString alloc] initWithBytes:value.c_str()
                                    length:value.size()
                                  encoding:NSUTF8StringEncoding];
  }
};

template <>
struct ObjcConverter<bool> {
  static BOOL ToObjc(bool value) {
    return value ? YES : NO;
  }
};

template <>
struct ObjcConverter<double> {
  static double ToObjc(double value) {
    return value;
  }
};

template <>
struct ObjcConverter<optional<bool>> {
  static BOOL ToObjc(optional<bool> value) {
    return value.has_value() ? *value : NO;
  }
};

template <>
struct ObjcConverter<optional<double>> {
  static double ToObjc(optional<double> value) {
    return value.has_value() ? *value : NAN;
  }
};

template <>
struct ObjcConverter<optional<std::string>> {
  static NSString *ToObjc(optional<std::string> value) {
    return value.has_value() ? ObjcConverter<std::string>::ToObjc(*value) : nil;
  }
};

template <typename T>
struct ObjcConverter<std::vector<T>> {
  using dest_type = decltype(ObjcConverter<T>::ToObjc(std::declval<T>()));

  static NSArray<dest_type> *ToObjc(const std::vector<T>& value) {
    NSMutableArray<dest_type>* ret =
        [[NSMutableArray<dest_type> alloc] initWithCapacity:value.size()];
    for (size_t i = 0; i < value.size(); i++)
      [ret addObject:ObjcConverter<T>::ToObjc(value[i])];
    return ret;
  }
};

}  // namespace util
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTIL_OBJC_UTILS_H_
