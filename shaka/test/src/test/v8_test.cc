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

#include "src/test/v8_test.h"

namespace shaka {

V8Test::V8Test() {}
V8Test::~V8Test() {}

void V8Test::SetUp() {
  engine_.reset(new JsEngine);
  setup_.reset(new JsEngine::SetupContext);
}

void V8Test::TearDown() {
  setup_.reset();
  engine_.reset();
}

}  // namespace shaka
