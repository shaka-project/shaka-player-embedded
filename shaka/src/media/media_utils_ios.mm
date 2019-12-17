// Copyright 2020 Google LLC
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

#include "src/media/media_utils.h"

#import <UIKit/UIKit.h>

namespace shaka {
namespace media {

std::pair<uint32_t, uint32_t> GetScreenResolution() {
  CGRect screenBounds = [[UIScreen mainScreen] bounds];
  CGFloat screenScale = [[UIScreen mainScreen] scale];
  return {static_cast<uint32_t>(screenBounds.size.width * screenScale),
          static_cast<uint32_t>(screenBounds.size.height * screenScale)};
}

}  // namespace media
}  // namespace shaka
