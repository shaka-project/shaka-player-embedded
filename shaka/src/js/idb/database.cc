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

#include "src/js/idb/database.h"

#include "src/core/js_manager_impl.h"
#include "src/js/idb/object_store.h"
#include "src/js/idb/transaction.h"
#include "src/js/js_error.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace idb {

namespace {

class DoCommit : public memory::Traceable {
 public:
  DoCommit(RefPtr<IDBTransaction> transaction,
           std::shared_ptr<SqliteConnection> connection)
      : trans_(transaction), connection_(connection) {}

  void Trace(memory::HeapTracer* tracer) const override {
    tracer->Trace(&trans_);
  }

  void operator()() {
    trans_->DoCommit(connection_.get());
  }

 private:
  Member<IDBTransaction> trans_;
  std::shared_ptr<SqliteConnection> connection_;
};

}  // namespace

DEFINE_STRUCT_SPECIAL_METHODS_COPYABLE(IDBObjectStoreParameters);

IDBDatabase::IDBDatabase(std::shared_ptr<SqliteConnection> connection,
                         const std::string& name, int64_t version,
                         const std::vector<std::string>& store_names)
    : db_name(name),
      object_store_names(new dom::DOMStringList(store_names)),
      version(version),
      connection_(connection) {
  AddListenerField(EventType::Abort, &on_abort);
  AddListenerField(EventType::Error, &on_error);
  AddListenerField(EventType::VersionChange, &on_version_change);
}

// \cond Doxygen_Skip
IDBDatabase::~IDBDatabase() {}
// \endcond Doxygen_Skip

void IDBDatabase::Trace(memory::HeapTracer* tracer) const {
  events::EventTarget::Trace(tracer);
  tracer->Trace(&object_store_names);
  tracer->Trace(&version_change_trans_);
}

ExceptionOr<RefPtr<IDBObjectStore>> IDBDatabase::CreateObjectStore(
    const std::string& name, optional<IDBObjectStoreParameters> parameters) {
  // 1. Let database be the database associated with this connection.
  // 2. Let transaction be database’s upgrade transaction if it is not null, or
  //    throw an "InvalidStateError" DOMException otherwise.
  if (!version_change_trans_)
    return JsError::DOMException(InvalidStateError);
  // 3. If transaction’s state is not active, then throw a
  //    "TransactionInactiveError" DOMException.
  if (!version_change_trans_->active ||
      !version_change_trans_->sqlite_transaction) {
    return JsError::DOMException(TransactionInactiveError);
  }
  // 4. Let keyPath be options’s keyPath member if it is not undefined or null,
  //    or null otherwise.
  // 5. If keyPath is not null and is not a valid key path, throw a
  //    "SyntaxError" DOMException.
  if (!parameters || !parameters->keyPath.empty() || !parameters->autoIncrement)
    return JsError::DOMException(NotSupportedError);

  // 6. If an object store named name already exists in database throw a
  //    "ConstraintError" DOMException.
  if (object_store_names->contains(name))
    return JsError::DOMException(ConstraintError);

  // 7. Let autoIncrement be options’s autoIncrement member.
  // 8. If autoIncrement is true and keyPath is an empty string or any sequence
  //    (empty or otherwise), throw an "InvalidAccessError" DOMException.

  // 9. Let store be a new object store in database. Set the created object
  //    store's name to name. If autoIncrement is true, then the created object
  //    store uses a key generator. If keyPath is not null, set the created
  //    object store's key path to keyPath.
  const auto status =
      version_change_trans_->sqlite_transaction->CreateObjectStore(db_name,
                                                                   name);
  if (status != DatabaseStatus::Success)
    return JsError::DOMException(UnknownError);
  object_store_names->emplace_back(name);
  version_change_trans_->AddObjectStore(name);

  // 10. Return a new object store handle associated with store and transaction.
  return new IDBObjectStore(version_change_trans_, name);
}

ExceptionOr<void> IDBDatabase::DeleteObjectStore(const std::string& name) {
  // 1. Let database be the database associated with this connection.
  // 2. Let transaction be database’s upgrade transaction if it is not null, or
  //    throw an "InvalidStateError" DOMException otherwise.
  if (!version_change_trans_)
    return JsError::DOMException(InvalidStateError);
  // 3. If transaction’s state is not active, then throw a
  //    "TransactionInactiveError" DOMException.
  if (!version_change_trans_->active ||
      !version_change_trans_->sqlite_transaction) {
    return JsError::DOMException(TransactionInactiveError);
  }
  // 4. Let store be the object store named name in database, or throw a
  //    "NotFoundError" DOMException if none.
  if (!object_store_names->contains(name))
    return JsError::DOMException(NotFoundError);

  // 5. Remove store from this connection's object store set.
  // 6. If there is an object store handle associated with store and
  //    transaction, remove all entries from its index set.
  // TODO: We don't do step 6, we usually throw a NotFoundError.
  // 7. Destroy store.
  const auto status =
      version_change_trans_->sqlite_transaction->DeleteObjectStore(db_name,
                                                                   name);
  if (status != DatabaseStatus::Success)
    return JsError::DOMException(UnknownError);
  util::RemoveElement(object_store_names.get(), name);
  version_change_trans_->DeleteObjectStore(name);

  return {};
}

ExceptionOr<RefPtr<IDBTransaction>> IDBDatabase::Transaction(
    variant<std::string, std::vector<std::string>> store_names,
    optional<IDBTransactionMode> mode) {
  // 1. If a running upgrade transaction is associated with the connection,
  //    throw an "InvalidStateError" DOMException.
  // 2. If the connection's close pending flag is true, throw an
  //    "InvalidStateError" DOMException.
  if (version_change_trans_ || close_pending_)
    return JsError::DOMException(InvalidStateError);

  // 3. Let scope be the set of unique strings in storeNames if it is a
  //    sequence, or a set containing one string equal to storeNames otherwise.
  std::vector<std::string> scope;
  if (holds_alternative<std::string>(store_names))
    scope.emplace_back(get<std::string>(store_names));
  else
    scope = get<std::vector<std::string>>(store_names);

  // 4. If any string in scope is not the name of an object store in the
  //    connected database, throw a "NotFoundError" DOMException.
  for (const std::string& name : scope) {
    if (!object_store_names->contains(name))
      return JsError::DOMException(NotFoundError);
  }
  // 5. If scope is empty, throw an "InvalidAccessError" DOMException.
  if (scope.empty())
    return JsError::DOMException(InvalidAccessError);

  // 6. If mode is not "readonly" or "readwrite", throw a TypeError.
  const IDBTransactionMode real_mode =
      mode.value_or(IDBTransactionMode::READ_ONLY);
  if (real_mode != IDBTransactionMode::READ_ONLY &&
      real_mode != IDBTransactionMode::READ_WRITE) {
    return JsError::TypeError(
        "Transaction mode must be 'readonly' or 'readwrite'");
  }

  // 7. Let transaction be a newly created transaction with connection, mode and
  //    the set of object stores named in scope.
  // 8. Set transaction’s cleanup event loop to the current event loop.
  RefPtr<IDBTransaction> ret = new IDBTransaction(this, real_mode, scope);

  JsManagerImpl::Instance()->MainThread()->AddInternalTask(
      TaskPriority::Internal, "IndexedDb Commit Transaction",
      DoCommit(ret, connection_));
  // 9. Return an IDBTransaction object representing transaction.
  return ret;
}

void IDBDatabase::Close() {
  if (!close_pending_) {
    close_pending_ = true;
  }
}


IDBDatabaseFactory::IDBDatabaseFactory() {
  AddReadOnlyProperty("name", &IDBDatabase::db_name);
  AddReadOnlyProperty("objectStoreNames", &IDBDatabase::object_store_names);
  AddReadOnlyProperty("version", &IDBDatabase::version);

  AddListenerField(EventType::Abort, &IDBDatabase::on_abort);
  AddListenerField(EventType::Error, &IDBDatabase::on_error);
  AddListenerField(EventType::VersionChange, &IDBDatabase::on_version_change);

  AddMemberFunction("createObjectStore", &IDBDatabase::CreateObjectStore);
  AddMemberFunction("deleteObjectStore", &IDBDatabase::DeleteObjectStore);
  AddMemberFunction("transaction", &IDBDatabase::Transaction);
  AddMemberFunction("close", &IDBDatabase::Close);
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
