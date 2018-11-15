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

#ifndef SHAKA_EMBEDDED_JS_WRAPPER_H_
#define SHAKA_EMBEDDED_JS_WRAPPER_H_

#include "src/core/ref_ptr.h"

namespace shaka {
namespace util {

template <typename T>
class JSWrapper {
 public:
  RefPtr<T> inner;

  JSWrapper() {}
  NON_COPYABLE_OR_MOVABLE_TYPE(JSWrapper);

  template <typename Func, typename... Args>
  auto CallInnerMethod(Func member, Args... args)
      -> decltype((inner->*member)(args...)) {
    DCHECK(inner) << "Must call Initialize.";
    return JsManagerImpl::Instance()
        ->MainThread()
        ->AddInternalTask(TaskPriority::Internal, "",
                          PlainCallbackTask(std::bind(member, inner, args...)))
        ->GetValue();
  }

  template <typename Var, typename Val>
  void SetMemberVariable(Var member, Val val) {
    DCHECK(inner) << "Must call Initialize.";
    auto callback = [this, member, val]() { inner->*member = val; };
    JsManagerImpl::Instance()
        ->MainThread()
        ->AddInternalTask(TaskPriority::Internal, "",
                          PlainCallbackTask(callback))
        ->GetValue();
  }

  template <typename Var>
  auto GetMemberVariable(Var member) -> decltype(inner->*member) {
    DCHECK(inner) << "Must call Initialize.";
    return JsManagerImpl::Instance()
        ->MainThread()
        ->AddInternalTask(TaskPriority::Internal, "",
                          PlainCallbackTask(std::bind(member, inner)))
        ->GetValue();
  }
};

}  // namespace util
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_WRAPPER_H_
