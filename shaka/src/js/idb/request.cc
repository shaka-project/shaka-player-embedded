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

#include "src/js/idb/request.h"

#include "src/js/dom/dom_exception.h"
#include "src/js/idb/cursor.h"
#include "src/js/idb/object_store.h"
#include "src/js/idb/transaction.h"
#include "src/mapping/enum.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace idb {

IDBRequest::IDBRequest(
    optional<variant<Member<IDBObjectStore>, Member<IDBCursor>>> source,
    RefPtr<IDBTransaction> transaction)
    : source(source), transaction(transaction) {
  AddListenerField(EventType::Success, &on_success);
  AddListenerField(EventType::Error, &on_error);
}

// \cond Doxygen_Skip
IDBRequest::~IDBRequest() {}
// \endcond Doxygen_Skip

void IDBRequest::Trace(memory::HeapTracer* tracer) const {
  events::EventTarget::Trace(tracer);
  tracer->Trace(&error_);
  tracer->Trace(&result_);
  tracer->Trace(&source);
  tracer->Trace(&transaction);
}

void IDBRequest::OnAbort() {
  CompleteError(JsError::DOMException(AbortError));
}

ExceptionOr<Any> IDBRequest::Result() const {
  if (ready_state != IDBRequestReadyState::DONE)
    return JsError::DOMException(InvalidStateError);
  return result_;
}

ExceptionOr<Any> IDBRequest::Error() const {
  if (ready_state != IDBRequestReadyState::DONE)
    return JsError::DOMException(InvalidStateError);
  return error_;
}

void IDBRequest::CompleteSuccess(Any result) {
  ready_state = IDBRequestReadyState::DONE;
  result_ = result;

  RefPtr<events::Event> event(new events::Event(EventType::Success));
  bool did_throw;
  DispatchEventInternal(event, &did_throw);
  if (did_throw)
    transaction->Abort();
}

void IDBRequest::CompleteError(JsError error) {
  ready_state = IDBRequestReadyState::DONE;
  error_.TryConvert(error.error());
  RaiseEvent<events::Event>(EventType::Error);
}

void IDBRequest::CompleteError(DatabaseStatus status) {
  switch (status) {
    case DatabaseStatus::NotFound:
      CompleteError(JsError::DOMException(NotFoundError));
      break;
    case DatabaseStatus::AlreadyExists:
      CompleteError(JsError::DOMException(DataError));
      break;
    case DatabaseStatus::Busy:
      CompleteError(JsError::DOMException(QuotaExceededError));
      break;
    case DatabaseStatus::BadVersionNumber:
      CompleteError(JsError::DOMException(VersionError));
      break;
    default:
      CompleteError(JsError::DOMException(UnknownError));
      break;
  }
}


IDBRequestFactory::IDBRequestFactory() {
  AddReadOnlyProperty("source", &IDBRequest::source);
  AddReadOnlyProperty("transaction", &IDBRequest::transaction);
  AddReadOnlyProperty("readyState", &IDBRequest::ready_state);

  AddGenericProperty("result", &IDBRequest::Result);
  AddGenericProperty("error", &IDBRequest::Error);

  AddListenerField(EventType::Success, &IDBRequest::on_success);
  AddListenerField(EventType::Error, &IDBRequest::on_error);
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
