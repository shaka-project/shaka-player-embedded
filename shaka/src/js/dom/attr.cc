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

#include "src/js/dom/attr.h"

#include "src/js/dom/document.h"
#include "src/js/dom/element.h"

namespace shaka {
namespace js {
namespace dom {

Attr::Attr(RefPtr<Document> document, RefPtr<Element> owner,
           const std::string& local_name, optional<std::string> namespace_uri,
           optional<std::string> namespace_prefix, const std::string& value)
    : Node(ATTRIBUTE_NODE, document),
      namespace_uri(namespace_uri),
      namespace_prefix(namespace_prefix),
      local_name(local_name),
      value(value),
      owner_element(owner) {
  DCHECK(!owner || owner->document() == document);
}

// \cond Doxygen_Skip
Attr::~Attr() {}
// \endcond Doxygen_Skip

std::string Attr::attr_name() const {
  if (!namespace_prefix.has_value())
    return local_name;
  return namespace_prefix.value() + ":" + local_name;
}

void Attr::Trace(memory::HeapTracer* tracer) const {
  Node::Trace(tracer);
  tracer->Trace(&owner_element);
}

std::string Attr::node_name() const {
  return attr_name();
}

optional<std::string> Attr::NodeValue() const {
  return value;
}

optional<std::string> Attr::TextContent() const {
  return value;
}


AttrFactory::AttrFactory() {
  AddReadOnlyProperty("namespaceURI", &Attr::namespace_uri);
  AddReadOnlyProperty("prefix", &Attr::namespace_prefix);
  AddReadOnlyProperty("localName", &Attr::local_name);
  AddReadOnlyProperty("value", &Attr::value);
  AddReadOnlyProperty("specified", &Attr::specified);
  AddReadOnlyProperty("ownerElement", &Attr::owner_element);

  AddGenericProperty("name", &Attr::attr_name);
}

}  // namespace dom
}  // namespace js
}  // namespace shaka
