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

#include "src/js/idb/transaction.h"

#include "src/core/js_manager_impl.h"
#include "src/js/dom/dom_exception.h"
#include "src/js/idb/database.h"
#include "src/js/idb/object_store.h"
#include "src/js/idb/request.h"
#include "src/js/js_error.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace idb {

IDBTransaction::IDBTransaction(RefPtr<IDBDatabase> db, IDBTransactionMode mode,
                               const std::vector<std::string>& scope)
    : db(db),
      error(nullptr),
      mode(mode),
      aborted(false),
      active(true),
      done(false),
      sqlite_transaction(nullptr) {
  AddListenerField(EventType::Abort, &on_abort);
  AddListenerField(EventType::Complete, &on_complete);
  AddListenerField(EventType::Error, &on_error);

  for (const std::string& name : scope) {
    scope_.emplace(name, new IDBObjectStore(this, name));
  }
}

// \cond Doxygen_Skip
IDBTransaction::~IDBTransaction() {}
// \endcond Doxygen_Skip

void IDBTransaction::Trace(memory::HeapTracer* tracer) const {
  events::EventTarget::Trace(tracer);
  tracer->Trace(&db);
  tracer->Trace(&error);
  tracer->Trace(&requests_);
  for (const auto& pair : scope_)
    tracer->Trace(&pair.second);
}

ExceptionOr<RefPtr<IDBObjectStore>> IDBTransaction::ObjectStore(
    const std::string name) {
  // 1. If transaction’s state is finished, then throw an "InvalidStateError"
  //    DOMException.
  if (done)
    return JsError::DOMException(InvalidStateError);
  // 2. Let store be the object store named name in this transaction's scope, or
  //    throw a "NotFoundError" DOMException if none.
  if (scope_.count(name) == 0)
    return JsError::DOMException(NotFoundError);

  // 3. Return an object store handle associated with store and this
  //    transaction.
  return scope_.at(name);
}

ExceptionOr<void> IDBTransaction::Abort() {
  // 1. If this transaction's state is committing or finished, then throw an
  //    "InvalidStateError" DOMException.
  if (done)
    return JsError::DOMException(InvalidStateError);
  aborted = true;
  active = false;
  return {};
}

RefPtr<IDBRequest> IDBTransaction::AddRequest(RefPtr<IDBRequest> request) {
  requests_.emplace_back(request);
  return request;
}

void IDBTransaction::DoCommit(SqliteConnection* connection) {
  DCHECK(JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  DCHECK(!done);

  SqliteTransaction transaction;
  DatabaseStatus status = connection->BeginTransaction(&transaction);
  if (status != DatabaseStatus::Success) {
    error = new dom::DOMException(UnknownError);
    aborted = true;
    active = false;
    RaiseEvent<events::Event>(EventType::Error);
  }
  DoCommit(&transaction);
}

void IDBTransaction::DoCommit(SqliteTransaction* transaction) {
  sqlite_transaction = transaction;

  for (auto it = requests_.begin(); it != requests_.end(); it++) {
    // These methods will call synchronously into JavaScript, which can add more
    // requests at the end.
    if (aborted)
      (*it)->OnAbort();
    else
      (*it)->PerformOperation(transaction);
    DCHECK((*it)->ready_state == IDBRequestReadyState::DONE);
  }

  sqlite_transaction = nullptr;
  active = false;
  done = true;

  const DatabaseStatus status =
      aborted ? transaction->Rollback() : transaction->Commit();
  if (status != DatabaseStatus::Success) {
    error = new dom::DOMException(UnknownError);
    aborted = true;
    RaiseEvent<events::Event>(EventType::Error);
  }

  if (aborted)
    RaiseEvent<events::Event>(EventType::Abort);
  else
    RaiseEvent<events::Event>(EventType::Complete);
}

void IDBTransaction::AddObjectStore(const std::string& name) {
  DCHECK_EQ(scope_.count(name), 0u);
  scope_[name] = new IDBObjectStore(this, name);
}

void IDBTransaction::DeleteObjectStore(const std::string& name) {
  DCHECK_EQ(scope_.count(name), 1u);
  scope_.erase(name);
}


IDBTransactionFactory::IDBTransactionFactory() {
  AddReadOnlyProperty("mode", &IDBTransaction::mode);
  AddReadOnlyProperty("db", &IDBTransaction::db);
  AddReadOnlyProperty("error", &IDBTransaction::error);

  AddListenerField(EventType::Abort, &IDBTransaction::on_abort);
  AddListenerField(EventType::Complete, &IDBTransaction::on_complete);
  AddListenerField(EventType::Error, &IDBTransaction::on_error);

  AddMemberFunction("objectStore", &IDBTransaction::ObjectStore);
  AddMemberFunction("abort", &IDBTransaction::Abort);

  NotImplemented("commit");
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
