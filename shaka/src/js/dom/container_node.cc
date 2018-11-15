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

#include "src/js/dom/container_node.h"

#include "src/js/dom/document.h"
#include "src/js/dom/element.h"
#include "src/js/js_error.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace dom {

namespace {

RefPtr<Element> ToElement(RefPtr<Node> node) {
  if (!node->is_element())
    return nullptr;
  return static_cast<Element*>(node.get());
}

}  // namespace

ContainerNode::ContainerNode(NodeType type, RefPtr<Document> document)
    : Node(type, document) {}

// \cond Doxygen_Skip
ContainerNode::~ContainerNode() {}
// \endcond Doxygen_Skip

std::vector<RefPtr<Element>> ContainerNode::GetElementsByTagName(
    const std::string& name) const {
  std::vector<RefPtr<Element>> ret;
  for (auto& child : child_nodes()) {
    RefPtr<Element> elem = ToElement(child);
    if (elem) {
      if (elem->tag_name() == name) {
        ret.push_back(elem);
      }

      auto temp = elem->GetElementsByTagName(name);
      ret.insert(ret.end(), temp.begin(), temp.end());
    }
  }

  return ret;
}


ContainerNodeFactory::ContainerNodeFactory() {
  AddMemberFunction("getElementsByTagName", &Element::GetElementsByTagName);

  NotImplemented("children");
  NotImplemented("firstElementChild");
  NotImplemented("lastElementChild");
  NotImplemented("childElementCount");

  NotImplemented("getElementsByTagNameNS");
  NotImplemented("getElementsByClassName");

  NotImplemented("prepend");
  NotImplemented("append");
  NotImplemented("querySelector");
  NotImplemented("querySelectorAll");
}

}  // namespace dom
}  // namespace js
}  // namespace shaka
