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

#ifndef SHAKA_EMBEDDED_JS_DOM_COMMENT_H_
#define SHAKA_EMBEDDED_JS_DOM_COMMENT_H_

#include <string>

#include "src/core/ref_ptr.h"
#include "src/js/dom/character_data.h"

namespace shaka {
namespace js {
namespace dom {

/**
 * Implements the Comment interface for DOM.
 * https://dom.spec.whatwg.org/#comment
 */
class Comment : public CharacterData {
  DECLARE_TYPE_INFO(Comment);

 public:
  Comment(RefPtr<Document> document, const std::string& data);

  std::string node_name() const override;
};

class CommentFactory : public BackingObjectFactory<Comment, CharacterData> {
 public:
  CommentFactory();
};

}  // namespace dom
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_DOM_COMMENT_H_
