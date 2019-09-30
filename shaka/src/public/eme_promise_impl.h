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

#ifndef SHAKA_EMBEDDED_EME_PROMISE_IMPL_H_
#define SHAKA_EMBEDDED_EME_PROMISE_IMPL_H_

#include <atomic>
#include <string>

#include "shaka/eme/eme_promise.h"

#include "src/core/ref_ptr.h"
#include "src/mapping/promise.h"
#include "src/util/macros.h"

namespace shaka {
namespace eme {

class EmePromise::Impl {
 public:
  Impl(const Promise& promise, bool has_value);
  virtual ~Impl();

  NON_COPYABLE_OR_MOVABLE_TYPE(Impl);

  virtual void Resolve();
  virtual void ResolveWith(bool value);
  virtual void Reject(ExceptionType except_type, const std::string& message);

 protected:
  Impl();

 private:
  RefPtr<Promise> promise_;
  std::atomic<bool> is_pending_;
  const bool has_value_;
};

}  // namespace eme
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_EME_PROMISE_IMPL_H_
