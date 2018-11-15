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

#include "src/js/url.h"

#include "src/js/mse/media_source.h"

namespace shaka {
namespace js {

URL::URL() {}
// \cond Doxygen_Skip
URL::~URL() {}
// \endcond Doxygen_Skip

std::string URL::CreateObjectUrl(RefPtr<mse::MediaSource> media_source) {
  return media_source->url;
}


URLFactory::URLFactory() {
  AddStaticFunction("createObjectURL", &URL::CreateObjectUrl);
}

}  // namespace js
}  // namespace shaka
