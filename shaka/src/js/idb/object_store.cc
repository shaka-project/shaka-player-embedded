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

#include "src/js/idb/object_store.h"

#include <utility>

#include "src/js/idb/database.h"
#include "src/js/idb/database.pb.h"
#include "src/js/idb/idb_utils.h"
#include "src/js/idb/request.h"
#include "src/js/idb/request_impls.h"
#include "src/js/idb/transaction.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace idb {

IDBObjectStore::IDBObjectStore(RefPtr<IDBTransaction> transaction,
                               const std::string& name)
    : store_name(name), transaction(transaction) {}
// \cond Doxygen_Skip
IDBObjectStore::~IDBObjectStore() {}
// \endcond Doxygen_Skip

void IDBObjectStore::Trace(memory::HeapTracer* tracer) const {
  BackingObject::Trace(tracer);
  tracer->Trace(&transaction);
}

ExceptionOr<RefPtr<IDBRequest>> IDBObjectStore::Add(Any value,
                                                    optional<IdbKeyType> key) {
  return AddOrPut(value, key, /* overwrite */ false);
}

ExceptionOr<RefPtr<IDBRequest>> IDBObjectStore::Put(Any value,
                                                    optional<IdbKeyType> key) {
  return AddOrPut(value, key, /* overwrite */ false);
}

ExceptionOr<RefPtr<IDBRequest>> IDBObjectStore::AddOrPut(
    Any value, optional<IdbKeyType> key, bool no_overwrite) {
  // 1-5
  RETURN_IF_ERROR(CheckState(/* need_write */ true));
  // 6. If store uses in-line keys and key was given, throw a "DataError"
  //    DOMException.
  if (key_path.has_value() && key.has_value())
    return JsError::DOMException(DataError);
  // 7. If store uses out-of-line keys and has no key generator and key was not
  //    given, throw a "DataError" DOMException.
  if (!key_path.has_value() && !auto_increment && !key.has_value())
    return JsError::DOMException(DataError);
  // 8. If key was given, then:
  // NA, already converted.

  // 9. Let targetRealm be a user-agent defined Realm.
  // 10. Let clone be a clone of value in targetRealm. Rethrow any exceptions.
  proto::Value clone;
  RETURN_IF_ERROR(StoreInProto(value, &clone));

  // 11. If store uses in-line keys, then:
  DCHECK(!key_path.has_value());

  // 12. Return the result (an IDBRequest) of running asynchronously execute a
  //     request with handle as source and store a record into an object store
  //     as operation, using store, the clone as value, key, and no-overwrite
  //     flag.
  return transaction->AddRequest(new IDBStoreRequest(
      this, transaction, std::move(clone), key, no_overwrite));
}

ExceptionOr<RefPtr<IDBRequest>> IDBObjectStore::Delete(IdbKeyType key) {
  // 1-5
  RETURN_IF_ERROR(CheckState(/* need_write */ true));
  // 6. Let range be the result of running convert a value to a key range with
  //    query and null disallowed flag true. Rethrow any exceptions.
  // NA, already converted.

  // 7. Return the result (an IDBRequest) of running asynchronously execute a
  //    request with this object store handle as source and delete records from
  //    an object store as operation, using store and range.
  return transaction->AddRequest(new IDBDeleteRequest(this, transaction, key));
}

ExceptionOr<RefPtr<IDBRequest>> IDBObjectStore::Get(IdbKeyType key) {
  // 1-4
  RETURN_IF_ERROR(CheckState(/* need_write */ false));
  // 5. Let range be the result of running convert a value to a key range with
  //    query and null disallowed flag true. Rethrow any exceptions.
  // NA, already converted.

  // 6. Return the result (an IDBRequest) of running asynchronously execute a
  //    request with this object store handle as source and retrieve a value
  //    from an object store as operation, using the current Realm as
  //    targetRealm, store and range.
  return transaction->AddRequest(new IDBGetRequest(this, transaction, key));
}

ExceptionOr<RefPtr<IDBRequest>> IDBObjectStore::OpenCursor(
    optional<IdbKeyType> range, optional<IDBCursorDirection> direction) {
  if (range)
    return JsError::DOMException(NotSupportedError);
  // 1-4
  RETURN_IF_ERROR(CheckState(/* need_write */ false));
  // 5. Let range be the result of running convert a value to a key range with
  //    query and null disallowed flag true. Rethrow any exceptions.
  // NA, already converted.
  // 6. Let cursor be a new cursor with its transaction set to transaction,
  //    undefined position, direction set to direction, got value flag set to
  //    false, undefined key and value, source set to store, range set to range,
  //    and key only flag set to false.
  const auto dir = direction.value_or(IDBCursorDirection::NEXT);
  RefPtr<IDBCursor> cursor = new IDBCursor(this, dir);
  // 7. Let request be the result of running asynchronously execute a request
  //    with this object store handle as source and iterate a cursor as
  //    operation, using the current Realm as targetRealm, and cursor.
  RefPtr<IDBIterateCursorRequest> request =
      new IDBIterateCursorRequest(this, transaction, cursor, 1);
  // 8. Set cursor’s request to request.
  cursor->request = request;
  // 9. Return request.
  return transaction->AddRequest(request);
}

ExceptionOr<void> IDBObjectStore::CheckState(bool need_write) const {
  // 1. Let transaction be this object store handle's transaction.
  // 2. Let store be this object store handle's object store.
  // 3. If store has been deleted, throw an "InvalidStateError" DOMException.
  if (!transaction->db->object_store_names->contains(store_name))
    return JsError::DOMException(InvalidStateError);
  // 4. If transaction’s state is not active, then throw a
  //    "TransactionInactiveError" DOMException.
  if (!transaction->active)
    return JsError::DOMException(TransactionInactiveError);
  // 5. If transaction is a read-only transaction, throw a "ReadOnlyError"
  //    DOMException.
  if (need_write && transaction->mode == IDBTransactionMode::READ_ONLY)
    return JsError::DOMException(ReadOnlyError);
  return {};
}


IDBObjectStoreFactory::IDBObjectStoreFactory() {
  AddReadOnlyProperty("autoIncrement", &IDBObjectStore::auto_increment);
  AddReadOnlyProperty("keyPath", &IDBObjectStore::key_path);
  AddReadOnlyProperty("name", &IDBObjectStore::store_name);
  AddReadOnlyProperty("transaction", &IDBObjectStore::transaction);

  AddMemberFunction("add", &IDBObjectStore::Add);
  AddMemberFunction("delete", &IDBObjectStore::Delete);
  AddMemberFunction("get", &IDBObjectStore::Get);
  AddMemberFunction("openCursor", &IDBObjectStore::OpenCursor);
  AddMemberFunction("put", &IDBObjectStore::Put);

  NotImplemented("clear");
  NotImplemented("count");
  NotImplemented("createIndex");
  NotImplemented("deleteIndex");
  NotImplemented("index");
  NotImplemented("indexNames");
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
