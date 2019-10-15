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

#ifndef SHAKA_EMBEDDED_EME_IMPLEMENTATION_FACTORY_H_
#define SHAKA_EMBEDDED_EME_IMPLEMENTATION_FACTORY_H_

#include <string>
#include <vector>

#include "../macros.h"
#include "configuration.h"

namespace shaka {
namespace eme {

class Implementation;
class ImplementationHelper;

/**
 * A factory used to create EME implementation instances and to query what this
 * implementation supports.  This is implemented by an app and registered with
 * the ImplementationRegistry.
 *
 * Methods on this type are only called on the JS main thread.
 *
 * @ingroup eme
 */
class SHAKA_EXPORT ImplementationFactory {
 public:
  // Since this is intended to be subclassed by the app, this type cannot be
  // changed without breaking ABI compatibility.  This includes adding
  // new virtual methods.

  virtual ~ImplementationFactory();

  /** @return Whether this implementation supports the given session type. */
  virtual bool SupportsSessionType(MediaKeySessionType type) const = 0;

  /** @return Whether this implementation supports the given init data type. */
  virtual bool SupportsInitDataType(MediaKeyInitDataType type) const = 0;

  /** @return Whether this implementation supports the given robustness. */
  virtual bool SupportsAudioRobustness(const std::string& robustness) const = 0;

  /** @return Whether this implementation supports the given robustness. */
  virtual bool SupportsVideoRobustness(const std::string& robustness) const = 0;

  /** @return The distinctive identifier requirements of the implementation. */
  virtual MediaKeysRequirement DistinctiveIdentifier() const = 0;

  /** @return The persistent state requirements of the implementation. */
  virtual MediaKeysRequirement PersistentState() const = 0;


  /**
   * Creates a new instance of the implementation.  The arguments have already
   * been filtered according to the support methods.  This should verify that
   * the arguments are compatible with the implementation.  If the
   * implementation doesn't support the given arguments, it MUST return nullptr.
   *
   * @param helper The helper instance used to callback to JavaScript.
   * @param distinctive_identifier The distinctive identifier requirement.
   * @param persistent_state The persistent state requirement.
   * @param audio_robustness The audio robustness requirements.
   * @param video_robustness The video robustness requirements.
   * @return A new implementation instance, or nullptr if not supported.
   */
  virtual Implementation* CreateImplementation(
      ImplementationHelper* helper,
      MediaKeysRequirement distinctive_identifier,
      MediaKeysRequirement persistent_state,
      const std::vector<std::string>& audio_robustness,
      const std::vector<std::string>& video_robustness) = 0;
};

}  // namespace eme
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_EME_IMPLEMENTATION_FACTORY_H_
