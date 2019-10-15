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

#ifndef SHAKA_EMBEDDED_EME_IMPLEMENTATION_REGISTRY_H_
#define SHAKA_EMBEDDED_EME_IMPLEMENTATION_REGISTRY_H_

#include <string>

#include "../macros.h"

namespace shaka {
namespace eme {

class ImplementationFactory;

/**
 * Defines a registry for implementations of EME.  During system startup all
 * implementations should be registered with this type to make them available.
 *
 * These methods are thread-safe.
 *
 * @ingroup eme
 */
class SHAKA_EXPORT ImplementationRegistry final {
 public:
  /**
   * Adds an EME implementation to the registry.  The caller retains ownership
   * of the pointer and it must remain valid for the entire lifetime of the
   * program.
   */
  static void AddImplementation(const std::string& key_system,
                                ImplementationFactory* factory);

  /** @returns The implementation of the given key system, or nullptr. */
  static ImplementationFactory* GetImplementation(
      const std::string& key_system);

 private:
  ImplementationRegistry();
};

}  // namespace eme
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_EME_IMPLEMENTATION_REGISTRY_H_
