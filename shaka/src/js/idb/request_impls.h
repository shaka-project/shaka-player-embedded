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

#ifndef SHAKA_EMBEDDED_JS_IDB_REQUEST_IMPLS_H_
#define SHAKA_EMBEDDED_JS_IDB_REQUEST_IMPLS_H_

#include "shaka/optional.h"
#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/js/idb/database.pb.h"
#include "src/js/idb/idb_utils.h"
#include "src/js/idb/request.h"

namespace shaka {
namespace js {
namespace idb {

class IDBCursor;

class IDBGetRequest : public IDBRequest {
 public:
  IDBGetRequest(
      optional<variant<Member<IDBObjectStore>, Member<IDBCursor>>> source,
      RefPtr<IDBTransaction> transaction, IdbKeyType key);
  ~IDBGetRequest() override;

  void PerformOperation(SqliteTransaction* transaction) override;

 private:
  const IdbKeyType key_;
};

class IDBStoreRequest : public IDBRequest {
 public:
  IDBStoreRequest(
      optional<variant<Member<IDBObjectStore>, Member<IDBCursor>>> source,
      RefPtr<IDBTransaction> transaction, proto::Value value,
      optional<IdbKeyType> key, bool no_override);
  ~IDBStoreRequest() override;

  void PerformOperation(SqliteTransaction* transaction) override;

 private:
  const proto::Value value_;
  const optional<IdbKeyType> key_;
  const bool no_override_;
};

class IDBDeleteRequest : public IDBRequest {
 public:
  IDBDeleteRequest(
      optional<variant<Member<IDBObjectStore>, Member<IDBCursor>>> source,
      RefPtr<IDBTransaction> transaction, IdbKeyType key);
  ~IDBDeleteRequest() override;

  void PerformOperation(SqliteTransaction* transaction) override;

 private:
  const IdbKeyType key_;
};

class IDBIterateCursorRequest : public IDBRequest {
 public:
  IDBIterateCursorRequest(
      optional<variant<Member<IDBObjectStore>, Member<IDBCursor>>> source,
      RefPtr<IDBTransaction> transaction, RefPtr<IDBCursor> cursor,
      uint32_t count);
  ~IDBIterateCursorRequest() override;

  void Trace(memory::HeapTracer* tracer) const override;
  void PerformOperation(SqliteTransaction* transaction) override;

  uint32_t count;

 private:
  const Member<IDBCursor> cursor_;
};

}  // namespace idb
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_IDB_REQUEST_IMPLS_H_
