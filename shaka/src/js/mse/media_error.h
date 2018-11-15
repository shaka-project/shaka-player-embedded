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

#ifndef SHAKA_EMBEDDED_JS_MSE_MEDIA_ERROR_H_
#define SHAKA_EMBEDDED_JS_MSE_MEDIA_ERROR_H_

#include <string>

#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/enum.h"

namespace shaka {
namespace js {
namespace mse {

enum MediaErrorCode {
  MEDIA_ERR_ABORTED = 1,
  MEDIA_ERR_NETWORK = 2,
  MEDIA_ERR_DECODE = 3,
  MEDIA_ERR_SRC_NOT_SUPPORTED = 4,
};

/**
 * An error in a media element.
 * @see https://html.spec.whatwg.org/multipage/media.html#mediaerror
 */
class MediaError : public BackingObject {
  DECLARE_TYPE_INFO(MediaError);

 public:
  MediaError(MediaErrorCode code, const std::string& message);

  const MediaErrorCode code;
  const std::string message;
};

class MediaErrorFactory : public BackingObjectFactory<MediaError> {
 public:
  MediaErrorFactory();
};

}  // namespace mse
}  // namespace js
}  // namespace shaka

CONVERT_ENUM_AS_NUMBER(shaka::js::mse, MediaErrorCode);

#endif  // SHAKA_EMBEDDED_JS_MSE_MEDIA_ERROR_H_
