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

#ifndef SHAKA_EMBEDDED_MAPPING_V8_V8_UTILS_H_
#define SHAKA_EMBEDDED_MAPPING_V8_V8_UTILS_H_

#include <v8.h>

#include <string>

namespace shaka {

/**
 * Gets the current Isolate instance, checking that we are in a valid state. V8
 * must be ready first, meaning that this must be the event thread or we are
 * being initialized (event loop is stopped).
 */
v8::Isolate* GetIsolate();

/** @return Wether the value is the value |true|. */
inline bool IsTrue(const v8::Maybe<bool>& value) {
  return value.IsJust() && value.FromJust();
}

/** Prints the given stack trace to stderr. */
void PrintStackTrace(const v8::Local<v8::StackTrace>& stack);

/**
 * Called when an uncaught exception occurs.  Prints information about the
 * given exception.
 */
void OnUncaughtException(const v8::Local<v8::Value>& exception,
                         bool in_promise);

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_V8_V8_UTILS_H_
