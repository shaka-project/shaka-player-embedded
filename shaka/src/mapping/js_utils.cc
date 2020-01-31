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

#include "src/mapping/js_utils.h"

#include <memory>

#include "src/core/js_manager_impl.h"
#include "src/core/task_runner.h"
#include "src/mapping/js_engine.h"

namespace shaka {

namespace {

js::JsError MakeError(const Error& error) {
  // Scheme plugins expect throwing shaka.util.Error instances.
  const double severity = error.severity != 0 ? error.severity : 2;  // CRITICAL
  LocalVar<JsValue> ctor = GetDescendant(JsEngine::Instance()->global_handle(),
                                         {"shaka", "util", "Error"});
  if (GetValueType(ctor) != proto::ValueType::Function) {
    LOG(DFATAL) << "Unable to find 'shaka.util.Error'";
    return js::JsError::TypeError(error.message);
  }
  LocalVar<JsFunction> ctor_func = UnsafeJsCast<JsFunction>(ctor);

  LocalVar<JsValue> args[] = {ToJsValue(severity), ToJsValue(error.category),
                              ToJsValue(error.code)};
  LocalVar<JsValue> results;
  if (!InvokeConstructor(ctor_func, 3, args, &results)) {
    LOG(DFATAL) << "Error creating shaka.util.Error: "
                << ConvertToString(results);
    return js::JsError::TypeError(error.message);
  }

  return js::JsError::Rethrow(results);
}

}  // namespace

ReturnVal<JsValue> GetDescendant(Handle<JsObject> root,
                                 const std::vector<std::string>& names) {
  if (names.empty())
    return RawToJsValue(root);

  LocalVar<JsObject> cur = root;
  for (size_t i = 0;; i++) {
    LocalVar<JsValue> child = GetMemberRaw(cur, names[i]);
    if (i == names.size() - 1)
      return child;
    if (!IsObject(child))
      return {};
    cur = UnsafeJsCast<JsObject>(child);
  }
}

void HandleNetworkFuture(Promise promise, std::future<optional<Error>> future,
                         std::function<void()> on_done) {
  auto shared_future = future.share();
  auto finish_future = [=]() {
    Promise copy = promise;  // By-value captures are const, so copy to edit.
    auto error = shared_future.get();
    if (error.has_value()) {
      copy.RejectWith(MakeError(error.value()), /* raise_events= */ false);
    } else {
      on_done();
    }
  };

  auto* thread = JsManagerImpl::Instance()->MainThread();
  if (shared_future.valid()) {
    // Add the user-defined literal for seconds.  "0s" is 0 seconds.
    using std::operator""s;
    if (shared_future.wait_for(0s) == std::future_status::timeout) {
      // If the future isn't ready yet, poll it on the JS main thread until
      // it is ready.
      std::shared_ptr<int> id(new int);
      auto poll = [=]() {
        if (shared_future.wait_for(0s) == std::future_status::timeout)
          return;
        thread->CancelTimer(*id);
        finish_future();
      };
      *id = thread->AddRepeatedTimer(250, PlainCallbackTask(std::move(poll)));
    } else {
      thread->AddInternalTask(TaskPriority::Internal, "",
                              PlainCallbackTask(std::move(finish_future)));
    }
  } else {
    thread->AddInternalTask(TaskPriority::Internal, "",
                            PlainCallbackTask(std::move(on_done)));
  }
}

}  // namespace shaka
