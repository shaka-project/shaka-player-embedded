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

#ifndef SHAKA_EMBEDDED_TEST_GLOBAL_FIELDS_H_
#define SHAKA_EMBEDDED_TEST_GLOBAL_FIELDS_H_

#include "shaka/media/default_media_player.h"
#include "shaka/js_manager.h"

namespace shaka {

// Defined and initialized in //shaka/test/main.cc

extern media::DefaultMediaPlayer* g_media_player;
extern JsManager* g_js_manager;

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_TEST_GLOBAL_FIELDS_H_
