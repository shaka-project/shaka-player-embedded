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

#include "src/js/dom/dom_parser.h"

#include "src/js/dom/document.h"
#include "src/js/dom/xml_document_parser.h"
#include "src/js/js_error.h"
#include "src/util/utils.h"

namespace shaka {
namespace js {
namespace dom {

DOMParser::DOMParser() {}
// \cond Doxygen_Skip
DOMParser::~DOMParser() {}
// \endcond Doxygen_Skip

ExceptionOr<RefPtr<Document>> DOMParser::ParseFromString(
    const std::string& source, const std::string& type) const {
  const std::string type_lower = util::ToAsciiLower(type);
  if (type_lower == "text/xml" || type_lower == "application/xml") {
    RefPtr<Document> ret = new Document();
    XMLDocumentParser parser(ret);
    return parser.Parse(source);
  }

  return JsError::TypeError("Unsupported parse type " + type);
}


DOMParserFactory::DOMParserFactory() {
  AddMemberFunction("parseFromString", &DOMParser::ParseFromString);
}

}  // namespace dom
}  // namespace js
}  // namespace shaka
