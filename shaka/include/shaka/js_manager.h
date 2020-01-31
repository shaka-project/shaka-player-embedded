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

#ifndef SHAKA_EMBEDDED_JS_MANAGER_H_
#define SHAKA_EMBEDDED_JS_MANAGER_H_

#include <memory>
#include <string>

#include "async_results.h"
#include "macros.h"

namespace shaka {
class JsManagerImpl;

/**
 * @defgroup exported Public Types
 * Types exported by the library.
 */

/**
 * @defgroup externs JavaScript Externs
 * @ingroup exported
 * Types defined in the JavaScript player and exposed directly.
 */

/**
 * @defgroup player Player Types
 * @ingroup exported
 * Types use for media playback.
 */

/**
 * Manages the JavaScript engine.  There must be exactly one instance per
 * program.  This manages a single V8 instance, but can support any number
 * of Player or Video instances.
 *
 * @ingroup player
 */
class SHAKA_EXPORT JsManager final {
 public:
  struct StartupOptions final {
    // This type is stack allocated, so the size is part of the public ABI;
    // fields can't be added without breaking compatibility.

    /**
     * The path to store persistent data (e.g. IndexedDB data).  This directory
     * needs write access, but can be initially empty.  It is assumed that we
     * have complete control over this directory (i.e. other programs won't
     * create/modify files here).
     *
     * If the path is relative, then it is relative to the working directory.
     */
    std::string dynamic_data_dir;

    /**
     * The path to static library data (e.g. shaka-player.compiled.js).  This
     * directory only needs read access.
     *
     * See |is_static_relative_to_bundle| for handling of relative paths.
     */
    std::string static_data_dir;

    /**
     * If set, then |static_data_dir| is relative to the iOS app bundle,
     * otherwise the path is relative to the working directory.  This flag is
     * ignored for non-iOS targets (always relative to working directory).
     */
    bool is_static_relative_to_bundle = false;
  };

  JsManager();
  JsManager(const StartupOptions& options);
  JsManager(JsManager&&);
  ~JsManager();

  JsManager& operator=(JsManager&&);

  SHAKA_NON_COPYABLE_TYPE(JsManager);

  /**
   * Stops the JavaScript engine and all background threads.  It is invalid
   * to call any methods on this class after this returns.
   */
  void Stop();

  /**
   * Blocks the current thread until all scheduled work is finished.  This is
   * used by the tests to detect when they are done.  This should not be called
   * if there are alive Player instances as they use setInterval which means
   * there will always be pending work.
   */
  void WaitUntilFinished();

  /**
   * Executes the given script in JavaScript.  This can be used to register
   * plugins or to run tests.  This cannot be called after Stop().  The script
   * will be scheduled to run on the event loop.
   */
  AsyncResults<void> RunScript(const std::string& path);

 private:
  std::unique_ptr<JsManagerImpl> impl_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_MANAGER_H_
