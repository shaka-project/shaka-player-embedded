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

#ifndef SHAKA_EMBEDDED_TEST_CORE_V8_TEST_H_
#define SHAKA_EMBEDDED_TEST_CORE_V8_TEST_H_

#include <gtest/gtest.h>

#include <memory>

#include "src/mapping/js_engine.h"
#include "src/memory/object_tracker.h"

namespace shaka {

/**
 * Defines a test that will initiate and cleanup an isolated JavaScript engine.
 * Each test gets its own engine.
 */
class V8Test : public testing::Test {
 public:
  V8Test();
  ~V8Test() override;

  void SetUp() override;
  void TearDown() override;

 protected:
#if defined(USING_V8)
  v8::Isolate* isolate() const {
    return engine_->isolate();
  }
#elif defined(USING_JSC)
  JSContextRef context() const {
    return engine_->context();
  }
#endif

 private:
  JsEngine::UnsetForTesting unset_engine_;
  memory::ObjectTracker::UnsetForTesting unset_tracker_;

  std::unique_ptr<JsEngine> engine_;
  // Store in a unique_ptr so it will live after SetUp returns.
  std::unique_ptr<JsEngine::SetupContext> setup_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_TEST_CORE_V8_TEST_H_
