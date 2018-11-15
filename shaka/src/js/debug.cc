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

#include "src/js/debug.h"

#include <chrono>
#include <thread>

namespace shaka {
namespace js {

Debug::Debug() {}
// \cond Doxygen_Skip
Debug::~Debug() {}
// \endcond Doxygen_Skip

std::string Debug::InternalTypeName(RefPtr<BackingObject> object) {
  return object->factory()->name();
}

std::string Debug::IndirectBases(RefPtr<BackingObject> object) {
  std::string ret;
  const BackingObjectFactoryBase* ptr = object->factory();
  while (ptr) {
    if (!ret.empty())
      ret.append(", ");
    ret.append(ptr->name());
    ptr = ptr->base();
  }
  return ret;
}

void Debug::Sleep(uint64_t delay_ms) {
  std::this_thread::sleep_for(std::chrono::microseconds(delay_ms));
}


DebugFactory::DebugFactory() {
  AddStaticFunction("internalTypeName", &Debug::InternalTypeName);
  AddStaticFunction("indirectBases", &Debug::IndirectBases);
  AddStaticFunction("sleep", &Debug::Sleep);
}


}  // namespace js
}  // namespace shaka
