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

#include "src/mapping/js_utils.h"

namespace shaka {

ReturnVal<JsValue> GetDescendant(Handle<JsObject> root,
                                 const std::vector<std::string>& names) {
  if (names.empty())
    return RawToJsValue(root);

  LocalVar<JsObject> cur = root;
  for (size_t i = 0;; i++) {
    LocalVar<JsValue> child = GetMemberRaw(cur, names[i]);
    if (i == names.size() - 1)
      return child;
    if (!IsObject(child))
      return {};
    cur = UnsafeJsCast<JsObject>(child);
  }
}

}  // namespace shaka
