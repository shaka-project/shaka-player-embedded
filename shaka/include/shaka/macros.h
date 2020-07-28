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

#ifndef SHAKA_EMBEDDED_MACROS_H_
#define SHAKA_EMBEDDED_MACROS_H_

#ifdef __cplusplus
#  define SHAKA_EXTERN_C extern "C"
#else
#  define SHAKA_EXTERN_C
#endif

#ifdef BUILDING_SHAKA
#  if __GNUC__ >= 4
#    define SHAKA_EXPORT __attribute__((visibility("default")))
#  elif defined(_WIN32)
#    define SHAKA_EXPORT __declspec(dllexport)
#  else
#    define SHAKA_EXPORT
#  endif
#else
#  ifdef _WIN32
#    define SHAKA_EXPORT __declspec(dllimport)
#  else
#    define SHAKA_EXPORT
#  endif
#endif

#define SHAKA_DECLARE_STRUCT_SPECIAL_METHODS(Type) \
  Type();                                          \
  Type(const Type&);                               \
  Type(Type&&);                                    \
  ~Type();                                         \
  Type& operator=(const Type&);                    \
  Type& operator=(Type&&)


#define SHAKA_NON_COPYABLE_TYPE(Type) \
  Type(const Type&) = delete;         \
  Type& operator=(const Type&) = delete

#define SHAKA_NON_MOVABLE_TYPE(Type) \
  Type(Type&&) = delete;        \
  Type& operator=(Type&&) = delete

#define SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(Type) \
  SHAKA_NON_COPYABLE_TYPE(Type);                 \
  SHAKA_NON_MOVABLE_TYPE(Type)

#define SHAKA_DECLARE_INTERFACE_METHODS(Type) \
  Type();                                     \
  virtual ~Type();                            \
  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(Type)


#endif  // SHAKA_EMBEDDED_MACROS_H_
