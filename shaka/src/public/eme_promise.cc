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

#include "shaka/eme/eme_promise.h"

#include <atomic>

#include "src/core/js_manager_impl.h"
#include "src/js/js_error.h"
#include "src/mapping/js_utils.h"
#include "src/mapping/promise.h"
#include "src/memory/heap_tracer.h"
#include "src/public/eme_promise_impl.h"

namespace shaka {
namespace eme {

EmePromise::Impl::Impl(const Promise& promise, bool has_value)
    : promise_(MakeJsRef<Promise>(promise)),
      is_pending_(false),
      has_value_(has_value) {}

EmePromise::Impl::Impl() : is_pending_(false), has_value_(false) {}

EmePromise::Impl::~Impl() {}

void EmePromise::Impl::Resolve() {
  bool expected = false;
  if (is_pending_.compare_exchange_strong(expected, true)) {
    if (has_value_) {
      LOG(WARNING) << "Resolve called on Promise that should have a "
                      "value, resolving with false.";
    }

    RefPtr<Promise> promise = promise_;
    bool has_value = has_value_;
    JsManagerImpl::Instance()->MainThread()->AddInternalTask(
        TaskPriority::Internal, "DoResolvePromise", [=]() {
          LocalVar<JsValue> value;
          if (has_value)
            value = ToJsValue(false);
          else
            value = JsUndefined();
          promise->ResolveWith(value);
        });
  }
}

void EmePromise::Impl::ResolveWith(bool value) {
  bool expected = false;
  if (is_pending_.compare_exchange_strong(expected, true)) {
    if (!has_value_) {
      LOG(WARNING) << "ResolveWith called on Promise that shouldn't have a "
                      "value, ignoring value.";
    }

    RefPtr<Promise> promise = promise_;
    bool has_value = has_value_;
    JsManagerImpl::Instance()->MainThread()->AddInternalTask(
        TaskPriority::Internal, "DoResolvePromise", [=]() {
          LocalVar<JsValue> js_value;
          if (has_value)
            js_value = ToJsValue(value);
          else
            js_value = JsUndefined();
          promise->ResolveWith(js_value);
        });
  }
}

void EmePromise::Impl::Reject(ExceptionType except_type,
                              const std::string& message) {
  bool expected = false;
  if (is_pending_.compare_exchange_strong(expected, true)) {
    RefPtr<Promise> promise = promise_;
    JsManagerImpl::Instance()->MainThread()->AddInternalTask(
        TaskPriority::Internal, "DoRejectPromise", [=]() {
          switch (except_type) {
            case ExceptionType::TypeError:
              promise->RejectWith(js::JsError::TypeError(message));
              break;
            case ExceptionType::RangeError:
              promise->RejectWith(js::JsError::RangeError(message));
              break;
            case ExceptionType::NotSupported:
              promise->RejectWith(
                  js::JsError::DOMException(NotSupportedError, message));
              break;
            case ExceptionType::InvalidState:
              promise->RejectWith(
                  js::JsError::DOMException(InvalidStateError, message));
              break;
            case ExceptionType::QuotaExceeded:
              promise->RejectWith(
                  js::JsError::DOMException(QuotaExceededError, message));
              break;

            default:
              promise->RejectWith(
                  js::JsError::DOMException(UnknownError, message));
              break;
          }
        });
  }
}


EmePromise::EmePromise() : impl_(nullptr) {}
EmePromise::EmePromise(const Promise& promise, bool has_value)
    : impl_(new Impl(promise, has_value)) {}
EmePromise::EmePromise(std::shared_ptr<Impl> impl) : impl_(impl) {}
EmePromise::EmePromise(const EmePromise& other) = default;
EmePromise::EmePromise(EmePromise&& other) = default;
EmePromise::~EmePromise() {}

EmePromise& EmePromise::operator=(const EmePromise& other) = default;
EmePromise& EmePromise::operator=(EmePromise&& other) = default;

bool EmePromise::valid() const {
  return impl_.get();
}

void EmePromise::Resolve() {
  impl_->Resolve();
}

void EmePromise::ResolveWith(bool value) {
  impl_->ResolveWith(value);
}

void EmePromise::Reject(ExceptionType except_type, const std::string& message) {
  impl_->Reject(except_type, message);
}

}  // namespace eme
}  // namespace shaka
