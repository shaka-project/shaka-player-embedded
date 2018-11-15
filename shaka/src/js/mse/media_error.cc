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

#include "src/js/mse/media_error.h"

namespace shaka {
namespace js {
namespace mse {

MediaError::MediaError(MediaErrorCode code, const std::string& message)
    : code(code), message(message) {}
// \cond Doxygen_Skip
MediaError::~MediaError() {}
// \endcond Doxygen_Skip


MediaErrorFactory::MediaErrorFactory() {
  AddConstant("MEDIA_ERR_ABORTED", MEDIA_ERR_ABORTED);
  AddConstant("MEDIA_ERR_NETWORK", MEDIA_ERR_NETWORK);
  AddConstant("MEDIA_ERR_DECODE", MEDIA_ERR_DECODE);
  AddConstant("MEDIA_ERR_SRC_NOT_SUPPORTED", MEDIA_ERR_SRC_NOT_SUPPORTED);

  AddReadOnlyProperty("code", &MediaError::code);
  AddReadOnlyProperty("message", &MediaError::message);
}

}  // namespace mse
}  // namespace js
}  // namespace shaka
