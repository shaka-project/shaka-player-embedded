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

#include "src/js/dom/node.h"

#include "src/js/dom/document.h"
#include "src/js/dom/element.h"
#include "src/js/js_error.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace dom {

Node::Node(NodeType type, RefPtr<Document> document)
    : owner_document_(document), node_type_(type) {
  DCHECK(!document.empty() || type == DOCUMENT_NODE);
}

// \cond Doxygen_Skip
Node::~Node() {}
// \endcond Doxygen_Skip

void Node::Trace(memory::HeapTracer* tracer) const {
  events::EventTarget::Trace(tracer);
  for (auto& child : children_)
    tracer->Trace(&child);
  tracer->Trace(&owner_document_);
  tracer->Trace(&parent_);
}

bool Node::IsShortLived() const {
  return true;
}

RefPtr<Document> Node::document() const {
  return owner_document_;
}

RefPtr<Node> Node::parent_node() const {
  return parent_;
}

std::vector<RefPtr<Node>> Node::child_nodes() const {
  return std::vector<RefPtr<Node>>(children_.begin(), children_.end());
}

RefPtr<Node> Node::first_child() const {
  return children_.empty() ? nullptr : children_.front();
}

RefPtr<Node> Node::last_child() const {
  return children_.empty() ? nullptr : children_.back();
}

RefPtr<Node> Node::AppendChild(RefPtr<Node> new_child) {
  DCHECK(is_element() || node_type_ == DOCUMENT_NODE);
  DCHECK(new_child);
  DCHECK(!new_child->parent_node());

  new_child->parent_ = this;
  children_.emplace_back(new_child);
  return new_child;
}


NodeFactory::NodeFactory() {
  AddConstant("ELEMENT_NODE", Node::ELEMENT_NODE);
  AddConstant("ATTRIBUTE_NODE", Node::ATTRIBUTE_NODE);
  AddConstant("TEXT_NODE", Node::TEXT_NODE);
  AddConstant("CDATA_SECTION_NODE", Node::CDATA_SECTION_NODE);
  AddConstant("ENTITY_REFERENCE_NODE", Node::ENTITY_REFERENCE_NODE);
  AddConstant("ENTITY_NODE", Node::ENTITY_NODE);
  AddConstant("PROCESSING_INSTRUCTION_NODE", Node::PROCESSING_INSTRUCTION_NODE);
  AddConstant("COMMENT_NODE", Node::COMMENT_NODE);
  AddConstant("DOCUMENT_NODE", Node::DOCUMENT_NODE);
  AddConstant("DOCUMENT_TYPE_NODE", Node::DOCUMENT_TYPE_NODE);
  AddConstant("DOCUMENT_FRAGMENT_NODE", Node::DOCUMENT_FRAGMENT_NODE);
  AddConstant("NOTATION_NODE", Node::NOTATION_NODE);

  AddConstant("NOTATION_NODE", Node::NOTATION_NODE);
  AddConstant("DOCUMENT_POSITION_DISCONNECTED",
              Node::DOCUMENT_POSITION_DISCONNECTED);
  AddConstant("DOCUMENT_POSITION_PRECEDING", Node::DOCUMENT_POSITION_PRECEDING);
  AddConstant("DOCUMENT_POSITION_FOLLOWING", Node::DOCUMENT_POSITION_FOLLOWING);
  AddConstant("DOCUMENT_POSITION_CONTAINS", Node::DOCUMENT_POSITION_CONTAINS);
  AddConstant("DOCUMENT_POSITION_CONTAINED_BY",
              Node::DOCUMENT_POSITION_CONTAINED_BY);
  AddConstant("DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC",
              Node::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);

  AddGenericProperty("ownerDocument", &Node::document);
  AddGenericProperty("nodeType", &Node::node_type);
  AddGenericProperty("nodeName", &Node::node_name);
  AddGenericProperty("parentNode", &Node::parent_node);
  AddGenericProperty("childNodes", &Node::child_nodes);
  AddGenericProperty("firstChild", &Node::first_child);
  AddGenericProperty("lastChild", &Node::last_child);
  AddGenericProperty("nodeValue", &Node::NodeValue);
  AddGenericProperty("textContent", &Node::TextContent);

  // Needed for testing, should not be used.
  AddMemberFunction("appendChild", &Node::AppendChild);

  NotImplemented("parentElement");
  NotImplemented("previousSibling");
  NotImplemented("nextSibling");

  NotImplemented("hasChildNodes");
  NotImplemented("getRootNode");
  NotImplemented("normalize");
  NotImplemented("contains");

  NotImplemented("insertBefore");
  NotImplemented("replaceChild");
  NotImplemented("removeChild");

  NotImplemented("isConnected");
  NotImplemented("baseURI");

  NotImplemented("cloneNode");
  NotImplemented("compareDocumentPosition");
  NotImplemented("lookupPrefix");
  NotImplemented("isDefaultNamespace");
  NotImplemented("isEqualNode");
  NotImplemented("isSameNode");
}

}  // namespace dom
}  // namespace js
}  // namespace shaka
