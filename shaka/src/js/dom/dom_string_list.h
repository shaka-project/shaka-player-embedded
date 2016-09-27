// Copyright 2019 Google LLC
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

#ifndef SHAKA_EMBEDDED_JS_DOM_DOM_STRING_LIST_H_
#define SHAKA_EMBEDDED_JS_DOM_DOM_STRING_LIST_H_

#include <string>
#include <vector>

#include "shaka/optional.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"

namespace shaka {
namespace js {
namespace dom {

class DOMStringList : public BackingObject, public std::vector<std::string> {
  DECLARE_TYPE_INFO(DOMStringList);

 public:
  DOMStringList();
  explicit DOMStringList(const std::vector<std::string>& copy);

  optional<std::string> item(uint32_t index) const;
  bool getter(size_t index, std::string* value) const;

  bool contains(const std::string& item) const;
};

class DOMStringListFactory : public BackingObjectFactory<DOMStringList> {
 public:
  DOMStringListFactory();
};

}  // namespace dom
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_DOM_DOM_STRING_LIST_H_
