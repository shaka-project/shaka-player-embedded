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

#include "src/js/dom/element.h"

#include "src/js/dom/attr.h"
#include "src/js/dom/document.h"
#include "src/js/dom/text.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace dom {

Element::Element(RefPtr<Document> document, const std::string& local_name,
                 optional<std::string> namespace_uri,
                 optional<std::string> namespace_prefix)
    : ContainerNode(ELEMENT_NODE, document),
      namespace_uri(namespace_uri),
      namespace_prefix(namespace_prefix),
      local_name(local_name) {}

// \cond Doxygen_Skip
Element::~Element() {}
// \endcond Doxygen_Skip

void Element::Trace(memory::HeapTracer* tracer) const {
  ContainerNode::Trace(tracer);
  tracer->Trace(&attributes_);
}

std::string Element::tag_name() const {
  if (!namespace_prefix.has_value())
    return local_name;
  return namespace_prefix.value() + ":" + local_name;
}

std::string Element::node_name() const {
  return tag_name();
}

optional<std::string> Element::NodeValue() const {
  return nullopt;
}

optional<std::string> Element::TextContent() const {
  std::string ret;
  for (auto& child : child_nodes()) {
    if (child->node_type() == TEXT_NODE) {
      ret.append(static_cast<Text*>(child.get())->data());
    } else if (child->is_element()) {
      auto temp = child->TextContent();
      DCHECK(temp.has_value());
      ret.append(temp.value());
    }
  }
  return ret;
}

optional<std::string> Element::GetAttribute(const std::string& name) const {
  auto it = FindAttribute(name);
  if (it == attributes_.end())
    return nullopt;
  return (*it)->value;
}

optional<std::string> Element::GetAttributeNS(const std::string& ns,
                                              const std::string& name) const {
  auto it = FindAttributeNS(ns, name);
  if (it == attributes_.end())
    return nullopt;
  return (*it)->value;
}

bool Element::HasAttribute(const std::string& name) const {
  return FindAttribute(name) != attributes_.end();
}

bool Element::HasAttributeNS(const std::string& ns,
                             const std::string& name) const {
  return FindAttributeNS(ns, name) != attributes_.end();
}

void Element::SetAttribute(const std::string& key, const std::string& value) {
  auto it = FindAttribute(key);
  if (it != attributes_.end()) {
    (*it)->value = value;
  } else {
    attributes_.emplace_back(
        new Attr(document(), this, key, nullopt, nullopt, value));
  }
}

void Element::SetAttributeNS(const std::string& ns, const std::string& key,
                             const std::string& value) {
  // |key| is a qualified name, e.g. 'foo:bar'.  We search by local name, so
  // split to get just 'bar'.
  auto split_at = key.find(':');
  CHECK_NE(split_at, std::string::npos);
  const std::string local_name = key.substr(split_at + 1);
  const std::string prefix = key.substr(0, split_at);

  auto it = FindAttributeNS(ns, local_name);
  if (it != attributes_.end()) {
    (*it)->value = value;
  } else {
    attributes_.push_back(
        new Attr(document(), this, local_name, ns, prefix, value));
  }
}

void Element::RemoveAttribute(const std::string& attr) {
  auto it = FindAttribute(attr);
  if (it != attributes_.end())
    attributes_.erase(it);
}

void Element::RemoveAttributeNS(const std::string& ns,
                                const std::string& attr) {
  auto it = FindAttributeNS(ns, attr);
  if (it != attributes_.end())
    attributes_.erase(it);
}

Element::attr_iter Element::FindAttribute(const std::string& name) {
  auto it = attributes_.begin();
  for (; it != attributes_.end(); it++) {
    if ((*it)->attr_name() == name)
      return it;
  }
  return it;
}

Element::attr_iter Element::FindAttributeNS(const std::string& ns,
                                            const std::string& name) {
  auto it = attributes_.begin();
  for (; it != attributes_.end(); it++) {
    if ((*it)->namespace_uri == ns && (*it)->local_name == name)
      return it;
  }
  return it;
}

std::vector<RefPtr<Attr>> Element::attributes() const {
  return std::vector<RefPtr<Attr>>(attributes_.begin(), attributes_.end());
}

ElementFactory::ElementFactory() {
  AddReadOnlyProperty("namespaceURI", &Element::namespace_uri);
  AddReadOnlyProperty("prefix", &Element::namespace_prefix);
  AddReadOnlyProperty("localName", &Element::local_name);
  AddReadOnlyProperty("id", &Element::id);

  AddGenericProperty("tagName", &Element::tag_name);

  AddMemberFunction("hasAttributes", &Element::has_attributes);
  AddMemberFunction("getAttribute", &Element::GetAttribute);
  AddMemberFunction("getAttributeNS", &Element::GetAttributeNS);
  AddMemberFunction("setAttribute", &Element::SetAttribute);
  AddMemberFunction("setAttributeNS", &Element::SetAttributeNS);
  AddMemberFunction("hasAttribute", &Element::HasAttribute);
  AddMemberFunction("hasAttributeNS", &Element::HasAttributeNS);
  AddMemberFunction("removeAttribute", &Element::RemoveAttribute);
  AddMemberFunction("removeAttributeNS", &Element::RemoveAttributeNS);

  AddGenericProperty("attributes", &Element::attributes);

  NotImplemented("className");
  NotImplemented("classList");
  NotImplemented("slot");

  NotImplemented("getAttributeNames");

  NotImplemented("getAttributeNode");
  NotImplemented("getAttributeNodeNS");
  NotImplemented("setAttributeNode");
  NotImplemented("setAttributeNodeNS");
  NotImplemented("removeAttributeNode");

  NotImplemented("attachShadow");
  NotImplemented("shadowRoot");

  NotImplemented("closest");
  NotImplemented("matches");
  NotImplemented("webkitMatchesSelector");

  NotImplemented("insertAdjacentElement");
  NotImplemented("insertAdjacentText");
}

}  // namespace dom
}  // namespace js
}  // namespace shaka
