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

#ifndef SHAKA_EMBEDDED_JS_DOM_XML_DOCUMENT_PARSER_H_
#define SHAKA_EMBEDDED_JS_DOM_XML_DOCUMENT_PARSER_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "shaka/optional.h"
#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/mapping/exception_or.h"

namespace shaka {
namespace js {
namespace dom {
class Node;
class Document;
class Text;

/**
 * Parses XML text data into a DOM tree.  All work is synchronous and no events
 * are fired.  This also is a strict parser, so it will reject documents that
 * some browsers may accept.
 *
 * The following features are not supported:
 * - Namespaces
 * - Events/mutators
 * - Attribute nodes (does not exist in our DOM).
 * - Processing instructions (does not exist in our DOM).
 */
class XMLDocumentParser {
 public:
  explicit XMLDocumentParser(RefPtr<Document> document);
  ~XMLDocumentParser();

  ExceptionOr<RefPtr<Document>> Parse(const std::string& source);

  // Callbacks from SAX
  void EndDocument();
  void StartElement(const std::string& local_name,
                    optional<std::string> namespace_uri,
                    optional<std::string> namespace_prefix,
                    size_t attribute_count,
                    const char** attributes);
  void EndElement();
  void Text(const std::string& text);
  void Comment(const std::string& text);
  void SetException(JsError error);

 private:
  /** If there is any cached text, create a new Text node for it. */
  void FinishTextNode();

  const Member<Document> document_;
  Member<Node> current_node_;
  std::string current_text_;
  std::unique_ptr<JsError> error_;
};

}  // namespace dom
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_DOM_XML_DOCUMENT_PARSER_H_
