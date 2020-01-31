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

#ifndef SHAKA_EMBEDDED_MAPPING_JS_UTILS_H_
#define SHAKA_EMBEDDED_MAPPING_JS_UTILS_H_

#include <functional>
#include <future>
#include <string>
#include <utility>
#include <vector>

#include "shaka/error.h"
#include "shaka/optional.h"
#include "src/core/ref_ptr.h"
#include "src/mapping/js_wrappers.h"
#include "src/mapping/promise.h"
#include "src/memory/object_tracker.h"

namespace shaka {

/**
 * Traverses a namespace/object structure to get a descendant member.  This will
 * repeatedly get the child of the current object with a name from @a names,
 * starting at @a root.  This will return an empty value if one of the children
 * isn't found or isn't an object.
 *
 * @param root The object to start traversing at.
 * @param names The member names to traverse with.
 * @return The descendant at the given path, or an empty value if not found.
 */
ReturnVal<JsValue> GetDescendant(Handle<JsObject> root,
                                 const std::vector<std::string>& names);

/**
 * Allocates a new heap object of the given type that is managed by the
 * ObjectTracker.  This allows non-Backing objects (e.g. Callback) to be managed
 * using RefPtr<T>.
 */
template <typename T, typename... Args>
RefPtr<T> MakeJsRef(Args&&... args) {
  auto* p = new T(std::forward<Args>(args)...);
  memory::ObjectTracker::Instance()->RegisterObject(p);
  return p;
}

/**
 * Handles watching the given future to finish, reports any errors to the
 * @a promise, and calls the given callback when (and if) the future resolves
 * with success.  The callback will always be invoked on the JS main thread.
 */
void HandleNetworkFuture(Promise promise, std::future<optional<Error>> future,
                         std::function<void()> on_done);

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_JS_UTILS_H_
