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

#include "src/js/idb/open_db_request.h"

#include <glog/logging.h>

#include <memory>
#include <vector>

#include "src/js/dom/dom_exception.h"
#include "src/js/events/version_change_event.h"
#include "src/js/idb/database.h"
#include "src/js/idb/object_store.h"
#include "src/js/idb/sqlite.h"
#include "src/js/idb/transaction.h"
#include "src/js/js_error.h"

namespace shaka {
namespace js {
namespace idb {

IDBOpenDBRequest::IDBOpenDBRequest(const std::string& name,
                                   optional<int64_t> version)
    : IDBRequest(nullopt, nullptr), name_(name), version_(version) {
  AddListenerField(EventType::UpgradeNeeded, &on_upgrade_needed);
}

// \cond Doxygen_Skip
IDBOpenDBRequest::~IDBOpenDBRequest() {}
// \endcond Doxygen_Skip

void IDBOpenDBRequest::DoOperation(const std::string& db_path) {
  std::shared_ptr<SqliteConnection> connection(new SqliteConnection(db_path));
  DatabaseStatus status = connection->Init();
  if (status != DatabaseStatus::Success)
    return CompleteError(status);

  SqliteTransaction transaction;
  status = connection->BeginTransaction(&transaction);
  if (status != DatabaseStatus::Success)
    return CompleteError(status);

  int64_t version = 0;
  status = transaction.GetDbVersion(name_, &version);
  const bool is_new = status == DatabaseStatus::NotFound;
  const int64_t new_version = version_.value_or(is_new ? 1 : version);
  if (!is_new && status != DatabaseStatus::Success)
    return CompleteError(status);
  if (new_version < version)
    return CompleteError(JsError::DOMException(VersionError));

  std::vector<std::string> store_names;
  if (!is_new) {
    status = transaction.ListObjectStores(name_, &store_names);
    if (status != DatabaseStatus::Success)
      return CompleteError(status);
  }

  RefPtr<IDBDatabase> idb_connection =
      new IDBDatabase(connection, name_, new_version, store_names);

  if (version != new_version) {
    if (is_new)
      status = transaction.CreateDb(name_, new_version);
    else
      status = transaction.UpdateDbVersion(name_, new_version);
    if (status != DatabaseStatus::Success)
      return CompleteError(status);

    RefPtr<IDBTransaction> idb_trans(new IDBTransaction(
        idb_connection, IDBTransactionMode::VERSION_CHANGE, store_names));
    idb_connection->VersionChangeTransaction(idb_trans);
    idb_trans->sqlite_transaction = &transaction;

    ready_state = IDBRequestReadyState::DONE;
    result_ = Any(idb_connection);
    this->transaction = idb_trans;
    auto* event = new events::IDBVersionChangeEvent(EventType::UpgradeNeeded,
                                                    version, new_version);
    bool did_throw;
    DispatchEventInternal(event, &did_throw);
    if (did_throw)
      idb_trans->Abort();

    idb_trans->DoCommit(&transaction);

    if (idb_trans->aborted || idb_connection->is_closed()) {
      if (!idb_connection->is_closed())
        idb_connection->Close();
      return CompleteError(JsError::DOMException(AbortError));
    }

    idb_connection->VersionChangeTransaction(nullptr);
  }

  CompleteSuccess(Any(idb_connection));
}

void IDBOpenDBRequest::PerformOperation(SqliteTransaction* /* transaction */) {
  LOG(FATAL) << "Not reached";
}


IDBOpenDBRequestFactory::IDBOpenDBRequestFactory() {
  AddListenerField(EventType::UpgradeNeeded,
                   &IDBOpenDBRequest::on_upgrade_needed);

  NotImplemented("onblocked");
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
