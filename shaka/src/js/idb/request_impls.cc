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

#include "src/js/idb/request_impls.h"

#include <string>
#include <utility>
#include <vector>

#include "src/js/idb/cursor.h"
#include "src/js/idb/database.h"
#include "src/js/idb/idb_utils.h"
#include "src/js/idb/object_store.h"
#include "src/js/idb/transaction.h"
#include "src/mapping/byte_string.h"

namespace shaka {
namespace js {
namespace idb {

namespace {

ExceptionOr<Any> ReadAndLoad(SqliteTransaction* transaction,
                             const std::string& db_name,
                             const std::string& store_name, IdbKeyType key,
                             bool allow_not_found) {
  std::vector<uint8_t> data;
  const DatabaseStatus status =
      transaction->GetData(db_name, store_name, key, &data);
  if (status == DatabaseStatus::NotFound) {
    if (allow_not_found)
      return Any();  // Undefined
    return JsError::DOMException(NotFoundError);
  }
  if (status != DatabaseStatus::Success)
    return JsError::DOMException(UnknownError);

  proto::Value proto;
  if (!proto.ParseFromArray(data.data(), data.size())) {
    return JsError::DOMException(UnknownError,
                                 "Invalid data stored in database");
  }

  return LoadFromProto(proto);
}

}  // namespace

IDBGetRequest::IDBGetRequest(
    optional<variant<Member<IDBObjectStore>, Member<IDBCursor>>> source,
    RefPtr<IDBTransaction> transaction, IdbKeyType key)
    : IDBRequest(source, transaction), key_(key) {}
IDBGetRequest::~IDBGetRequest() {}

void IDBGetRequest::PerformOperation(SqliteTransaction* transaction) {
  RefPtr<IDBObjectStore> store = get<Member<IDBObjectStore>>(source.value());
  RefPtr<IDBDatabase> db = store->transaction->db;

  ExceptionOr<Any> data =
      ReadAndLoad(transaction, db->db_name, store->store_name, key_,
                  /* allow_not_found */ true);
  if (holds_alternative<JsError>(data))
    return CompleteError(get<JsError>(std::move(data)));
  return CompleteSuccess(get<Any>(data));
}


IDBStoreRequest::IDBStoreRequest(
    optional<variant<Member<IDBObjectStore>, Member<IDBCursor>>> source,
    RefPtr<IDBTransaction> transaction, proto::Value value,
    optional<IdbKeyType> key, bool no_override)
    : IDBRequest(source, transaction),
      value_(std::move(value)),
      key_(key),
      no_override_(no_override) {}
IDBStoreRequest::~IDBStoreRequest() {}

void IDBStoreRequest::PerformOperation(SqliteTransaction* transaction) {
  RefPtr<IDBObjectStore> store = get<Member<IDBObjectStore>>(source.value());
  RefPtr<IDBDatabase> db = store->transaction->db;

  DatabaseStatus status;
  if (key_.has_value()) {
    std::vector<uint8_t> ignored;
    status = transaction->GetData(db->db_name, store->store_name, key_.value(),
                                  &ignored);
    if (status == DatabaseStatus::Success) {
      if (no_override_) {
        return CompleteError(JsError::DOMException(
            ConstraintError, "An object with the given key already exists"));
      }
    } else if (status != DatabaseStatus::NotFound) {
      return CompleteError(status);
    }
  }

  std::string data;
  if (!value_.SerializeToString(&data))
    return CompleteError(JsError::DOMException(UnknownError));
  std::vector<uint8_t> data_vec(data.begin(), data.end());
  IdbKeyType key{};
  if (key_.has_value()) {
    status = transaction->UpdateData(db->db_name, store->store_name,
                                     key_.value(), data_vec);
  } else {
    status =
        transaction->AddData(db->db_name, store->store_name, data_vec, &key);
  }
  if (status != DatabaseStatus::Success)
    return CompleteError(status);
  return CompleteSuccess(Any(key_.value_or(key)));
}


IDBDeleteRequest::IDBDeleteRequest(
    optional<variant<Member<IDBObjectStore>, Member<IDBCursor>>> source,
    RefPtr<IDBTransaction> transaction, IdbKeyType key)
    : IDBRequest(source, transaction), key_(key) {}
IDBDeleteRequest::~IDBDeleteRequest() {}

void IDBDeleteRequest::PerformOperation(SqliteTransaction* transaction) {
  RefPtr<IDBObjectStore> store = get<Member<IDBObjectStore>>(source.value());
  RefPtr<IDBDatabase> db = store->transaction->db;

  const DatabaseStatus status =
      transaction->DeleteData(db->db_name, store->store_name, key_);
  if (status != DatabaseStatus::Success)
    return CompleteError(status);
  return CompleteSuccess(Any());  // undefined
}


IDBIterateCursorRequest::IDBIterateCursorRequest(
    optional<variant<Member<IDBObjectStore>, Member<IDBCursor>>> source,
    RefPtr<IDBTransaction> transaction, RefPtr<IDBCursor> cursor,
    uint32_t count)
    : IDBRequest(source, transaction), count(count), cursor_(cursor) {}
IDBIterateCursorRequest::~IDBIterateCursorRequest() {}

void IDBIterateCursorRequest::Trace(memory::HeapTracer* tracer) const {
  IDBRequest::Trace(tracer);
  tracer->Trace(&cursor_);
}

void IDBIterateCursorRequest::PerformOperation(SqliteTransaction* transaction) {
  RefPtr<IDBObjectStore> store = get<Member<IDBObjectStore>>(source.value());
  RefPtr<IDBDatabase> db = store->transaction->db;

  optional<int64_t> position = cursor_->key;
  const bool ascending = cursor_->direction == IDBCursorDirection::NEXT ||
                         cursor_->direction == IDBCursorDirection::NEXT_UNIQUE;
  for (uint32_t i = 0; i < count; i++) {
    int64_t new_key;
    const DatabaseStatus status = transaction->FindData(
        db->db_name, store->store_name, position, ascending, &new_key);
    if (status == DatabaseStatus::NotFound) {
      cursor_->key = nullopt;
      cursor_->value = Any();
      return CompleteSuccess(Any(nullptr));
    }
    if (status != DatabaseStatus::Success) {
      return CompleteError(status);
    }
    position = new_key;
  }

  ExceptionOr<Any> data =
      ReadAndLoad(transaction, db->db_name, store->store_name, position.value(),
                  /* allow_not_found */ false);
  if (holds_alternative<JsError>(data))
    return CompleteError(get<JsError>(std::move(data)));

  cursor_->key = position;
  cursor_->value = get<Any>(data);
  cursor_->got_value = true;
  return CompleteSuccess(Any(cursor_));
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
