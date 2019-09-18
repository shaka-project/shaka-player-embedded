// Copyright 2019 Google LLC
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

#include "src/js/idb/delete_db_request.h"

#include <glog/logging.h>

#include <memory>

#include "src/js/events/version_change_event.h"
#include "src/js/idb/database.h"
#include "src/js/idb/object_store.h"
#include "src/js/idb/sqlite.h"
#include "src/js/idb/transaction.h"
#include "src/js/js_error.h"

namespace shaka {
namespace js {
namespace idb {

IDBDeleteDBRequest::IDBDeleteDBRequest(const std::string& name)
    : IDBRequest(nullopt, nullptr), name_(name) {}

void IDBDeleteDBRequest::DoOperation(const std::string& db_path) {
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
  if (status != DatabaseStatus::Success && status != DatabaseStatus::NotFound)
    return CompleteError(status);

  if (status != DatabaseStatus::NotFound) {
    status = transaction.DeleteDb(name_);
    if (status != DatabaseStatus::Success)
      return CompleteError(status);

    status = transaction.Commit();
    if (status != DatabaseStatus::Success)
      return CompleteError(status);
  }

  // Don't use CompleteSuccess so we can fire a special event instead.
  ready_state = IDBRequestReadyState::DONE;
  RaiseEvent<events::IDBVersionChangeEvent>(EventType::Success, version,
                                            nullopt);
}

void IDBDeleteDBRequest::PerformOperation(
    SqliteTransaction* /* transaction */) {
  LOG(FATAL) << "Not reached";
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
