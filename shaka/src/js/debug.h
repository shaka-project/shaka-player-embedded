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

#ifndef SHAKA_EMBEDDED_JS_DEBUG_H_
#define SHAKA_EMBEDDED_JS_DEBUG_H_

#include <string>

#include "src/core/ref_ptr.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"

namespace shaka {
namespace js {

/**
 * Defines a number of internal, project-specific JavaScript methods used to
 * help debug the project.
 */
class Debug : public BackingObject {
  DECLARE_TYPE_INFO(Debug);

 public:
  Debug();

  static std::string InternalTypeName(RefPtr<BackingObject> object);
  static std::string IndirectBases(RefPtr<BackingObject> object);

  static void Sleep(uint64_t delay_ms);
};

class DebugFactory : public BackingObjectFactory<Debug> {
 public:
  DebugFactory();
};

}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_DEBUG_H_
