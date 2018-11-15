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
#include "src/mapping/promise.h"
#include "src/memory/heap_tracer.h"
#include "src/public/eme_promise_impl.h"

namespace shaka {
namespace eme {

namespace {

/**
 * A task that will resolve the given Promise with the given value.
 */
class DoResolvePromise : public memory::Traceable {
 public:
  DoResolvePromise(Promise promise, bool has_value, bool value)
      : promise_(promise), has_value_(has_value), value_(value) {}

  void Trace(memory::HeapTracer* tracer) const override {
    tracer->Trace(&promise_);
  }

  void operator()() {
    LocalVar<JsValue> value;
    if (has_value_)
      value = ToJsValue(value_);
    else
      value = JsUndefined();
    promise_.ResolveWith(value);
  }

 private:
  Promise promise_;
  const bool has_value_;
  const bool value_;
};

/**
 * A task that will reject the given Promise with the given value.
 */
class DoRejectPromise : public memory::Traceable {
 public:
  DoRejectPromise(Promise promise, ExceptionType type,
                  const std::string& message)
      : promise_(promise), message_(message), type_(type) {}

  void Trace(memory::HeapTracer* tracer) const override {
    tracer->Trace(&promise_);
  }

  void operator()() {
    switch (type_) {
      case ExceptionType::TypeError:
        promise_.RejectWith(js::JsError::TypeError(message_));
        break;
      case ExceptionType::RangeError:
        promise_.RejectWith(js::JsError::RangeError(message_));
        break;
      case ExceptionType::NotSupported:
        promise_.RejectWith(
            js::JsError::DOMException(NotSupportedError, message_));
        break;
      case ExceptionType::InvalidState:
        promise_.RejectWith(
            js::JsError::DOMException(InvalidStateError, message_));
        break;
      case ExceptionType::QuotaExceeded:
        promise_.RejectWith(
            js::JsError::DOMException(QuotaExceededError, message_));
        break;

      default:
        promise_.RejectWith(js::JsError::DOMException(UnknownError, message_));
        break;
    }
  }

 private:
  Promise promise_;
  const std::string message_;
  const ExceptionType type_;
};

}  // namespace

EmePromise::Impl::Impl(const Promise& promise, bool has_value)
    : is_pending_(false), has_value_(has_value) {
  // We need to register the object before we store in RefPtr<T>.
  auto* p = new Promise(promise);
  memory::ObjectTracker::Instance()->RegisterObject(p);
  promise_.reset(p);
}

EmePromise::Impl::Impl() : is_pending_(false), has_value_(false) {}

EmePromise::Impl::~Impl() {}

void EmePromise::Impl::Resolve() {
  bool expected = false;
  if (is_pending_.compare_exchange_strong(expected, true)) {
    if (has_value_) {
      LOG(WARNING) << "Resolve called on Promise that should have a "
                      "value, resolving with false.";
    }

    JsManagerImpl::Instance()->MainThread()->AddInternalTask(
        TaskPriority::Internal, "DoResolvePromise",
        DoResolvePromise(*promise_, has_value_, false));
  }
}

void EmePromise::Impl::ResolveWith(bool value) {
  bool expected = false;
  if (is_pending_.compare_exchange_strong(expected, true)) {
    if (!has_value_) {
      LOG(WARNING) << "ResolveWith called on Promise that shouldn't have a "
                      "value, ignoring value.";
    }

    JsManagerImpl::Instance()->MainThread()->AddInternalTask(
        TaskPriority::Internal, "DoResolvePromise",
        DoResolvePromise(*promise_, has_value_, value));
  }
}

void EmePromise::Impl::Reject(ExceptionType except_type,
                              const std::string& message) {
  bool expected = false;
  if (is_pending_.compare_exchange_strong(expected, true)) {
    JsManagerImpl::Instance()->MainThread()->AddInternalTask(
        TaskPriority::Internal, "DoRejectPromise",
        DoRejectPromise(*promise_, except_type, message));
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
