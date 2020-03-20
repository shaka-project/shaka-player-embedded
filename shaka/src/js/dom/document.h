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

#ifndef SHAKA_EMBEDDED_JS_DOM_DOCUMENT_H_
#define SHAKA_EMBEDDED_JS_DOM_DOCUMENT_H_

#include <atomic>
#include <string>
#include <vector>

#include "shaka/optional.h"
#include "src/js/dom/container_node.h"

namespace shaka {
namespace js {
namespace dom {

class Attr;
class Comment;
class Element;
class Text;

/**
 * Implements the Document interface for DOM.
 * https://dom.spec.whatwg.org/#document
 */
class Document : public ContainerNode {
  DECLARE_TYPE_INFO(Document);

 public:
  Document();

  static Document* Create() {
    return new Document();
  }
  static RefPtr<Document> EnsureGlobalDocument();

  /**
   * @return The time, in milliseconds, the document was created, according to
   *   Clock's getMonotonicTime.
   */
  uint64_t created_at() const { return created_at_; }

  std::string node_name() const override;
  optional<std::string> NodeValue() const override;
  optional<std::string> TextContent() const override;

  RefPtr<Element> DocumentElement() const;

  std::vector<RefPtr<Element>> GetElementsByTagName(
      const std::string& name) const override;

  RefPtr<Element> CreateElement(const std::string& name);
  RefPtr<Comment> CreateComment(const std::string& data);
  RefPtr<Text> CreateTextNode(const std::string& data);

  ExceptionOr<RefPtr<Attr>> CreateAttribute(const std::string& name);
  ExceptionOr<RefPtr<Attr>> CreateAttributeNS(
      const std::string& namespace_uri, const std::string& qualified_name);

 private:
  static std::atomic<Document*> instance_;
  const uint64_t created_at_;
};

class DocumentFactory : public BackingObjectFactory<Document, ContainerNode> {
 public:
  DocumentFactory();
};

}  // namespace dom
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_DOM_DOCUMENT_H_
