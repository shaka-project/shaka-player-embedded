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

#ifndef SHAKA_EMBEDDED_JS_DOM_CHARACTER_DATA_H_
#define SHAKA_EMBEDDED_JS_DOM_CHARACTER_DATA_H_

#include <string>

#include "shaka/optional.h"
#include "src/js/dom/node.h"

namespace shaka {
namespace js {
namespace dom {

/**
 * Implements the CharacterData interface for DOM.
 * https://dom.spec.whatwg.org/#characterdata
 */
class CharacterData : public Node {
  DECLARE_TYPE_INFO(CharacterData);

 public:
  CharacterData(NodeType type, RefPtr<Document> document);
  CharacterData(NodeType type, RefPtr<Document> document,
                const std::string& data);

  optional<std::string> NodeValue() const override;
  optional<std::string> TextContent() const override;

  size_t length() const {
    return data_.size();
  }
  std::string data() const {
    return data_;
  }
  void SetData(const std::string& data);

 private:
  std::string data_;
};

class CharacterDataFactory : public BackingObjectFactory<CharacterData, Node> {
 public:
  CharacterDataFactory();
};

}  // namespace dom
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_DOM_CHARACTER_DATA_H_
