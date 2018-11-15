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

#ifndef SHAKA_EMBEDDED_JS_DOM_DOM_PARSER_H_
#define SHAKA_EMBEDDED_JS_DOM_DOM_PARSER_H_

#include <string>

#include "src/core/ref_ptr.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/exception_or.h"

namespace shaka {
namespace js {
namespace dom {
class Document;

/**
 * Implements the DOMParser interface for DOM.
 * https://w3c.github.io/DOM-Parsing/#the-domparser-interface
 */
class DOMParser : public BackingObject {
  DECLARE_TYPE_INFO(DOMParser);

 public:
  DOMParser();

  static DOMParser* Create() {
    return new DOMParser();
  }

  /**
   * Parses the given string into a document.  Only supports XML documents.
   * The spec says this should always return a non-null document; however to
   * make errors more obvious (and make parsing easier), this throws in the
   * event of error.
   */
  ExceptionOr<RefPtr<Document>> ParseFromString(const std::string& source,
                                                const std::string& type) const;
};

class DOMParserFactory : public BackingObjectFactory<DOMParser> {
 public:
  DOMParserFactory();
};

}  // namespace dom
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_DOM_DOM_PARSER_H_
