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

#include "src/js/dom/text.h"

#include <deque>

#include "src/js/dom/container_node.h"
#include "src/js/dom/document.h"

namespace shaka {
namespace js {
namespace dom {

Text::Text(RefPtr<Document> document, const std::string& data)
    : CharacterData(TEXT_NODE, document, data) {}

// \cond Doxygen_Skip
Text::~Text() {}
// \endcond Doxygen_Skip

std::string Text::node_name() const {
  return "#text";
}


TextFactory::TextFactory() {
  NotImplemented("splitText");
  NotImplemented("wholeText");
}

}  // namespace dom
}  // namespace js
}  // namespace shaka
