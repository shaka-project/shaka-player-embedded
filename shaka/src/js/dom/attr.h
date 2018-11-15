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

#ifndef SHAKA_EMBEDDED_JS_DOM_ATTR_H_
#define SHAKA_EMBEDDED_JS_DOM_ATTR_H_

#include <string>

#include "shaka/optional.h"
#include "src/js/dom/node.h"

namespace shaka {
namespace js {
namespace dom {
class Element;

/**
 * Implements the Attr interface for DOM.
 * https://dom.spec.whatwg.org/#attr
 */
class Attr final : public Node {
  DECLARE_TYPE_INFO(Attr);

 public:
  Attr(RefPtr<Element> owner, const std::string& local_name,
       optional<std::string> namespace_uri,
       optional<std::string> namespace_prefix,
       const std::string& value);

  const optional<std::string> namespace_uri;
  const optional<std::string> namespace_prefix;
  const std::string local_name;
  const bool specified = true;
  std::string value;

  const Member<Element> owner_element;

  std::string attr_name() const;

  void Trace(memory::HeapTracer* tracer) const override;

  std::string node_name() const override;
  optional<std::string> NodeValue() const override;
  optional<std::string> TextContent() const override;
};

class AttrFactory : public BackingObjectFactory<Attr, Node> {
 public:
  AttrFactory();
};

}  // namespace dom
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_DOM_ATTR_H_
