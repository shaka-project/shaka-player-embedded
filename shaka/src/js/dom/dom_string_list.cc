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

#include "src/js/dom/dom_string_list.h"

namespace shaka {
namespace js {
namespace dom {

DOMStringList::DOMStringList() {}
DOMStringList::DOMStringList(const std::vector<std::string>& copy)
    : std::vector<std::string>(copy) {}
DOMStringList::~DOMStringList() {}

optional<std::string> DOMStringList::item(uint32_t index) const {
  if (index >= size())
    return nullopt;
  return (*this)[index];
}

bool DOMStringList::getter(size_t index, std::string* value) const {
  if (index >= size())
    return false;
  *value = (*this)[index];
  return true;
}

bool DOMStringList::contains(const std::string& item) const {
  return util::contains(*this, item);
}


DOMStringListFactory::DOMStringListFactory() {
  AddIndexer(&DOMStringList::getter);

  AddGenericProperty<DOMStringList>("length", &DOMStringList::size);

  AddMemberFunction("item", &DOMStringList::item);
  AddMemberFunction("contains", &DOMStringList::contains);
}

}  // namespace dom
}  // namespace js
}  // namespace shaka
