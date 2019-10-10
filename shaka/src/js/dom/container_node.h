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

#ifndef SHAKA_EMBEDDED_JS_DOM_CONTAINER_NODE_H_
#define SHAKA_EMBEDDED_JS_DOM_CONTAINER_NODE_H_

#include <string>
#include <vector>

#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/mapping/exception_or.h"
#include "src/js/dom/node.h"

namespace shaka {
namespace js {
namespace dom {
class Element;

/**
 * Defines a Node that has children.  This defines a common base class between
 * Element and Document.
 *
 * This also implements the ParentNode mixin:
 * https://dom.spec.whatwg.org/#parentnode
 */
class ContainerNode : public Node {
  DECLARE_TYPE_INFO(ContainerNode);

 public:
  ContainerNode(NodeType type, RefPtr<Document> document);

  std::vector<RefPtr<Element>> GetElementsByTagName(
      const std::string& name) const;
  ExceptionOr<RefPtr<Element>> QuerySelector(const std::string& query) const;
};

class ContainerNodeFactory : public BackingObjectFactory<ContainerNode, Node> {
 public:
  ContainerNodeFactory();
};

}  // namespace dom
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_DOM_CONTAINER_NODE_H_
