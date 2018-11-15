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

#ifndef SHAKA_EMBEDDED_JS_DOM_NODE_H_
#define SHAKA_EMBEDDED_JS_DOM_NODE_H_

#include <string>
#include <vector>

#include "shaka/optional.h"
#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/js/events/event_target.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/enum.h"

namespace shaka {
namespace js {
namespace dom {
class Document;
class Element;

/**
 * Implements the Node interface for DOM.
 * https://dom.spec.whatwg.org/#interface-node
 *
 * IMPORTANT: This does not implement any event firing.  Because the player
 * only uses XML parsing, it does not need it.  Meaning that altering the
 * DOM tree will not result in any mutation events.
 *
 * This also does not implement shadow-DOM.
 *
 * The DOM spec says that Nodes should return the same objects, namely that
 * adding a field to a node should persist.  However, here we do not persist
 * backing objects if they are not used.
 *
 * \code{js}
 * // JavaScript
 * var x = document.getElementById('abc');
 * x.firstChild.foobar = 'xyz';
 * gc();
 * console.log(x.firstChild.foobar);
 * \endcode
 *
 * The above should work, but may not because the GC will collect the firstCild
 * wrapper (JavaScript) object since it has no references.
 */
class Node : public events::EventTarget {
  DECLARE_TYPE_INFO(Node);

 public:
  enum NodeType {
    ELEMENT_NODE = 1,
    ATTRIBUTE_NODE = 2,
    TEXT_NODE = 3,
    CDATA_SECTION_NODE = 4,
    ENTITY_REFERENCE_NODE = 5,  // historical
    ENTITY_NODE = 6,            // historical
    PROCESSING_INSTRUCTION_NODE = 7,
    COMMENT_NODE = 8,
    DOCUMENT_NODE = 9,
    DOCUMENT_TYPE_NODE = 10,
    DOCUMENT_FRAGMENT_NODE = 11,
    NOTATION_NODE = 12,  // historical
  };
  enum DocumentPosition {
    DOCUMENT_POSITION_DISCONNECTED = 0x01,
    DOCUMENT_POSITION_PRECEDING = 0x02,
    DOCUMENT_POSITION_FOLLOWING = 0x04,
    DOCUMENT_POSITION_CONTAINS = 0x08,
    DOCUMENT_POSITION_CONTAINED_BY = 0x10,
    DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC = 0x20,
  };

  Node(NodeType type, RefPtr<Document> document);

  void Trace(memory::HeapTracer* tracer) const override;
  bool IsShortLived() const override;

  // Generic getters.
  RefPtr<Document> document() const;
  RefPtr<Node> parent_node() const;
  std::vector<RefPtr<Node>> child_nodes() const;
  NodeType node_type() const {
    return node_type_;
  }
  virtual std::string node_name() const = 0;
  virtual optional<std::string> NodeValue() const = 0;
  virtual optional<std::string> TextContent() const = 0;

  RefPtr<Node> first_child() const;
  RefPtr<Node> last_child() const;

  RefPtr<Node> AppendChild(RefPtr<Node> new_child);

  // Internal only methods.
  bool is_document() const {
    return node_type_ == DOCUMENT_NODE;
  }
  bool is_element() const {
    return node_type_ == ELEMENT_NODE;
  }
  bool is_char_data() const {
    return node_type_ == TEXT_NODE ||
           node_type_ == PROCESSING_INSTRUCTION_NODE ||
           node_type_ == COMMENT_NODE;
  }

 private:
  std::vector<Member<Node>> children_;
  Member<Node> parent_;
  const Member<Document> owner_document_;
  const NodeType node_type_;
};

class NodeFactory : public BackingObjectFactory<Node, events::EventTarget> {
 public:
  NodeFactory();
};

}  // namespace dom
}  // namespace js
}  // namespace shaka

CONVERT_ENUM_AS_NUMBER(shaka::js::dom::Node, NodeType);
CONVERT_ENUM_AS_NUMBER(shaka::js::dom::Node, DocumentPosition);

#endif  // SHAKA_EMBEDDED_JS_DOM_NODE_H_
