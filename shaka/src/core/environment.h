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

#ifndef SHAKA_EMBEDDED_CORE_ENVIRONMENT_H_
#define SHAKA_EMBEDDED_CORE_ENVIRONMENT_H_

#include <memory>

namespace shaka {

/**
 * Manages the JavaScript global environment.  This installs functions and
 * global objects like Navigator, XMLHttpRequest, and MediaSource.  This also
 * holds the factories used to create instances.  This object must live as long
 * as the JavaScript engine is being used.
 */
class Environment {
 public:
  Environment();
  ~Environment();

  /** Populates the global environment into the current V8 isolate. */
  void Install();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_CORE_ENVIRONMENT_H_
