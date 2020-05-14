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

#include "test/src/test/js_test_fixture.h"

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <string>

#include "src/core/js_manager_impl.h"
#include "src/mapping/any.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/callback.h"
#include "src/mapping/promise.h"
#include "src/mapping/register_member.h"

namespace shaka {

namespace {

/**
 * A helper that is used to keep a Callback object alive.  This will have a
 * non-zero ref count so the ObjectTracker will trace this and keep the
 * Callback alive.
 */
class CallbackHolder : public BackingObject {
 public:
  static std::string name() {
    return "CallbackHolder";
  }

  CallbackHolder(Callback callback) : callback(callback) {}

  void Trace(memory::HeapTracer* tracer) const override {
    tracer->Trace(&callback);
  }

  BackingObjectFactoryBase* factory() const override {
    LOG(FATAL) << "Not reached";
  }

  Callback callback;
};

class TestImpl : public testing::Test {
 public:
  TestImpl(RefPtr<CallbackHolder> callback) : callback_(callback) {}

  void TestBody() override {
    std::promise<void> test_done;
    auto task = [this, &test_done]() {
      LocalVar<JsValue> value = callback_->callback.ToJsValue();
      LocalVar<JsFunction> func = UnsafeJsCast<JsFunction>(value);
      LocalVar<JsValue> result;
      if (!InvokeMethod(func, JsEngine::Instance()->global_handle(), 0, nullptr,
                        &result)) {
        ADD_FAILURE() << GetStack(result);
        test_done.set_value();
        return;
      }

      if (GetValueType(result) == proto::ValueType::Undefined) {
        test_done.set_value();
        return;
      }

      Promise promise;
      if (!promise.TryConvert(result)) {
        ADD_FAILURE() << "Unable to convert return value to Promise";
        test_done.set_value();
        return;
      }

      promise.Then([&](Any) { test_done.set_value(); },
                   [&](Any err) {
                     LocalVar<JsValue> val = err.ToJsValue();
                     ADD_FAILURE() << GetStack(val);
                     test_done.set_value();
                   });
    };
    JsManagerImpl::Instance()->MainThread()->AddInternalTask(
        TaskPriority::Immediate, "", std::move(task));
    test_done.get_future().get();
  }

 private:
  std::string GetStack(Handle<JsValue> except) {
    if (IsObject(except)) {
      LocalVar<JsObject> obj = UnsafeJsCast<JsObject>(except);
      LocalVar<JsValue> stack = GetMemberRaw(obj, "stack");
      return ConvertToString(stack);
    }
    return ConvertToString(except);
  }

  RefPtr<CallbackHolder> callback_;
};

class TestFactory : public testing::internal::TestFactoryBase {
 public:
  TestFactory(Callback callback)
      : impl_(new TestImpl(new CallbackHolder(callback))) {}

  testing::Test* CreateTest() override {
    CHECK(impl_);
    auto* ret = impl_;
    impl_ = nullptr;
    return ret;
  }

 private:
  TestImpl* impl_;
};

void DefineTest(const std::string& test_name, Callback callback) {
  // Use gtest internals to dynamically register a new test case.
  testing::internal::MakeAndRegisterTestInfo(
      "JsTests", test_name.c_str(), nullptr, nullptr,
      testing::internal::CodeLocation("", 0),
      testing::internal::GetTestTypeId(), &TestImpl::SetUpTestCase,
      &TestImpl::TearDownTestCase, new TestFactory(callback));
}

void Fail(const std::string& message, const std::string& file, int line) {
  ADD_FAILURE_AT(file.c_str(), line) << message;
}

void TestSkip() {
  GTEST_SKIP();
}

}  // namespace

void RegisterTestFixture() {
  RegisterGlobalFunction("testSkip", &TestSkip);
  RegisterGlobalFunction("test_", &DefineTest);
  RegisterGlobalFunction("fail_", &Fail);
}

}  // namespace shaka
