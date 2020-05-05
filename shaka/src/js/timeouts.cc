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

#include "src/js/timeouts.h"

#include <algorithm>
#include <utility>

#include "src/core/js_manager_impl.h"
#include "src/mapping/js_utils.h"
#include "src/mapping/register_member.h"

namespace shaka {
namespace js {

// The minimum delay (in milliseconds) that a timer can be set to.
constexpr const uint64_t kMinTimerDelay = 4;

void Timeouts::Install() {
  RegisterGlobalFunction("setTimeout", &Timeouts::SetTimeout);
  RegisterGlobalFunction("setInterval", &Timeouts::SetInterval);
  RegisterGlobalFunction("clearTimeout", &Timeouts::ClearTimeout);
  RegisterGlobalFunction("clearInterval", &Timeouts::ClearInterval);
}

int Timeouts::SetTimeout(Callback callback, optional<uint64_t> timeout) {
  const uint64_t real_timeout = std::max(timeout.value_or(0), kMinTimerDelay);
  RefPtr<Callback> cb = MakeJsRef<Callback>(std::move(callback));
  return JsManagerImpl::Instance()->MainThread()->AddTimer(real_timeout,
                                                           [=]() { (*cb)(); });
}

int Timeouts::SetInterval(Callback callback, optional<uint64_t> timeout) {
  const uint64_t real_timeout = std::max(timeout.value_or(0), kMinTimerDelay);
  RefPtr<Callback> cb = MakeJsRef<Callback>(std::move(callback));
  return JsManagerImpl::Instance()->MainThread()->AddRepeatedTimer(
      real_timeout, [=]() { (*cb)(); });
}

void Timeouts::ClearTimeout(optional<int> id) {
  if (id.has_value())
    JsManagerImpl::Instance()->MainThread()->CancelTimer(id.value());
}

void Timeouts::ClearInterval(optional<int> id) {
  if (id.has_value())
    JsManagerImpl::Instance()->MainThread()->CancelTimer(id.value());
}

}  // namespace js
}  // namespace shaka
