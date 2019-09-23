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
#include <utility>
#include <vector>

#include "shaka/async_results.h"
#include "shaka/error_objc.h"
#include "shaka/optional.h"
#include "shaka/variant.h"
#include "src/public/error_objc+Internal.h"

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


template <typename... Args>
void DispatchObjcEvent(__weak id weak_client, SEL selector, Args... args) {
  // See https://stackoverflow.com/a/20058585
  dispatch_async(dispatch_get_main_queue(), ^{
    NSObject *client = weak_client;
    if (client && [client respondsToSelector:selector]) {
      IMP imp = [client methodForSelector:selector];
      auto func = reinterpret_cast<void (*)(id, SEL, Args...)>(imp);
      func(client, selector, args...);
    }
  });
}

namespace impl {

template <typename Ret>
struct BlockInvoker {
  template <typename Func>
  static void Invoke(const AsyncResults<Ret>& future, Func block) {
    using T = decltype(ObjcConverter<Ret>::ToObjc(std::declval<Ret>()));
    if (future.has_error())
      block(T(), [[ShakaPlayerError alloc] initWithError:future.error()]);
    else
      block(ObjcConverter<Ret>::ToObjc(future.results()), nil);
  }
};
template <>
struct BlockInvoker<void> {
  template <typename Func>
  static void Invoke(const AsyncResults<void>& future, Func block) {
    if (future.has_error())
      block([[ShakaPlayerError alloc] initWithError:future.error()]);
    else
      block(nil);
  }
};

}  // namespace impl

template <typename This, typename Ret, typename Func>
void CallBlockForFuture(This that, AsyncResults<Ret> future, Func block) {
  __block AsyncResults<Ret> local_future = std::move(future);
  dispatch_async(
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
      ^{
        // Wait on a background thread; then, once we have a value, call the
        // block on the main thread.
        local_future.wait();
        dispatch_async(dispatch_get_main_queue(), ^{
          // Keep a reference to "this" so the Objective-C object remains alive
          // until the callback is done.
          This other = that;
          (void)other;

          impl::BlockInvoker<Ret>::Invoke(local_future, block);
        });
      });
}

}  // namespace util
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_UTIL_OBJC_UTILS_H_
