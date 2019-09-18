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

#include "src/js/idb/cursor.h"

#include "src/js/idb/database.h"
#include "src/js/idb/object_store.h"
#include "src/js/idb/request.h"
#include "src/js/idb/request_impls.h"
#include "src/js/idb/transaction.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace idb {

IDBCursor::IDBCursor(RefPtr<IDBObjectStore> source, IDBCursorDirection dir)
    : source(source), direction(dir) {}
// \cond Doxygen_Skip
IDBCursor::~IDBCursor() {}
// \endcond Doxygen_Skip

void IDBCursor::Trace(memory::HeapTracer* tracer) const {
  BackingObject::Trace(tracer);
  tracer->Trace(&source);
  tracer->Trace(&request);
  tracer->Trace(&value);
}

ExceptionOr<void> IDBCursor::Continue(optional<Any> key) {
  // 1. Let transaction be this cursor's transaction.
  RefPtr<IDBTransaction> transaction = request->transaction;
  // 2. If transaction’s state is not active, then throw a
  //    "TransactionInactiveError" DOMException.
  if (!transaction->active)
    return JsError::DOMException(TransactionInactiveError);
  // 3. If the cursor’s source or effective object store has been deleted, throw
  //    an "InvalidStateError" DOMException.
  RefPtr<IDBDatabase> db = transaction->db;
  if (!db->object_store_names->contains(source->store_name))
    return JsError::DOMException(InvalidStateError);
  // 4. If this cursor’s got value flag is false, indicating that the cursor is
  //    being iterated or has iterated past its end, throw an
  //    "InvalidStateError" DOMException.
  if (!got_value)
    return JsError::DOMException(InvalidStateError);
  // 5. If key is given, then:
  if (key.has_value())
    return JsError::DOMException(NotSupportedError);

  // 6. Set this cursor's got value flag to false.
  got_value = false;
  // 7. Let request be this cursor's request.
  // 8. Set request’s processed flag to false.
  // 9. Set request’s done flag to false.

  // 10. Run asynchronously execute a request with the cursor’s source as
  //     source, iterate a cursor as operation and request, using the current
  //     Realm as targetRealm, this cursor and key (if given).
  // IMPL: Just add it back to the list to execute again.
  request->count = 1;
  transaction->AddRequest(request);
  return {};
}

ExceptionOr<RefPtr<IDBRequest>> IDBCursor::Delete() {
  // 1. Let transaction be this cursor's transaction.
  RefPtr<IDBTransaction> transaction = request->transaction;
  // 2. If transaction’s state is not active, then throw a
  //    "TransactionInactiveError" DOMException.
  if (!transaction->active)
    return JsError::DOMException(TransactionInactiveError);
  // 3. If transaction is a read-only transaction, throw a "ReadOnlyError"
  //    DOMException.
  if (transaction->mode == IDBTransactionMode::READ_ONLY)
    return JsError::DOMException(ReadOnlyError);
  // 4. If the cursor’s source or effective object store has been deleted, throw
  //    an "InvalidStateError" DOMException.
  RefPtr<IDBDatabase> db = transaction->db;
  if (!db->object_store_names->contains(source->store_name))
    return JsError::DOMException(InvalidStateError);
  // 5. If this cursor’s got value flag is false, indicating that the cursor is
  //    being iterated or has iterated past its end, throw an
  //    "InvalidStateError" DOMException.
  if (!got_value)
    return JsError::DOMException(InvalidStateError);
  DCHECK(key.has_value());
  // 6. If this cursor’s key only flag is true, throw an "InvalidStateError"
  //    DOMException.
  // 7. Return the result (an IDBRequest) of running asynchronously execute a
  //    request with this cursor as source and delete records from an object
  //    store as operation, using this cursor’s effective object store and
  //    effective key as store and key respectively.
  return transaction->AddRequest(
      new IDBDeleteRequest(source, transaction, key.value()));
}


IDBCursorFactory::IDBCursorFactory() {
  AddReadOnlyProperty("direction", &IDBCursor::direction);
  AddReadOnlyProperty("key", &IDBCursor::key);
  AddReadOnlyProperty("primaryKey", &IDBCursor::key);
  AddReadOnlyProperty("request", &IDBCursor::request);
  AddReadOnlyProperty("source", &IDBCursor::source);
  AddReadOnlyProperty("value", &IDBCursor::value);

  AddMemberFunction("continue", &IDBCursor::Continue);
  AddMemberFunction("delete", &IDBCursor::Delete);

  NotImplemented("advance");
  NotImplemented("update");
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
