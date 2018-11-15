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

#ifndef SHAKA_EMBEDDED_JS_DOM_ELEMENT_H_
#define SHAKA_EMBEDDED_JS_DOM_ELEMENT_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "shaka/optional.h"
#include "src/js/dom/container_node.h"

namespace shaka {
namespace js {
namespace dom {
class Attr;

/**
 * Implements the Element interface for DOM.
 * https://dom.spec.whatwg.org/#element
 */
class Element : public ContainerNode {
  DECLARE_TYPE_INFO(Element);

 public:
  Element(RefPtr<Document> document, const std::string& local_name,
          optional<std::string> namespace_uri,
          optional<std::string> namespace_prefix);

  void Trace(memory::HeapTracer* tracer) const override;

  const optional<std::string> namespace_uri;
  const optional<std::string> namespace_prefix;
  const std::string local_name;
  const std::string id;

  std::string tag_name() const;

  std::string node_name() const override;
  optional<std::string> NodeValue() const override;
  optional<std::string> TextContent() const override;

  bool has_attributes() const {
    return !attributes_.empty();
  }
  optional<std::string> GetAttribute(const std::string& name) const;
  optional<std::string> GetAttributeNS(const std::string& ns,
                                       const std::string& name) const;
  void SetAttribute(const std::string& key, const std::string& value);
  void SetAttributeNS(const std::string& ns, const std::string& key,
                      const std::string& value);
  bool HasAttribute(const std::string& name) const;
  bool HasAttributeNS(const std::string& ns, const std::string& name) const;
  void RemoveAttribute(const std::string& attr);
  void RemoveAttributeNS(const std::string& ns, const std::string& attr);

 private:
  using attr_iter = std::vector<Member<Attr>>::iterator;
  using const_attr_iter = std::vector<Member<Attr>>::const_iterator;

  attr_iter FindAttribute(const std::string& name);
  const_attr_iter FindAttribute(const std::string& name) const {
    return const_cast<Element*>(this)->FindAttribute(name);
  }

  attr_iter FindAttributeNS(const std::string& ns, const std::string& name);
  const_attr_iter FindAttributeNS(const std::string& ns,
                                  const std::string& name) const {
    return const_cast<Element*>(this)->FindAttributeNS(ns, name);
  }

  std::vector<Member<Attr>> attributes_;
};

class ElementFactory : public BackingObjectFactory<Element, ContainerNode> {
 public:
  ElementFactory();
};

}  // namespace dom
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_DOM_ELEMENT_H_
