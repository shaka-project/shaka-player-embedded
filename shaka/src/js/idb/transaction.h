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

#ifndef SHAKA_EMBEDDED_JS_IDB_TRANSACTION_H_
#define SHAKA_EMBEDDED_JS_IDB_TRANSACTION_H_

#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/js/events/event_target.h"
#include "src/js/idb/sqlite.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/enum.h"
#include "src/mapping/exception_or.h"

namespace shaka {
namespace js {

namespace dom {
class DOMException;
}  // namespace dom

namespace idb {

class IDBDatabase;
class IDBObjectStore;
class IDBRequest;

enum class IDBTransactionMode {
  READ_ONLY,
  READ_WRITE,
  VERSION_CHANGE,
};

class IDBTransaction : public events::EventTarget {
  DECLARE_TYPE_INFO(IDBTransaction);

 public:
  IDBTransaction(RefPtr<IDBDatabase> db, IDBTransactionMode mode,
                 const std::vector<std::string>& scope);

  void Trace(memory::HeapTracer* tracer) const override;

  ExceptionOr<RefPtr<IDBObjectStore>> ObjectStore(const std::string name);
  ExceptionOr<void> Abort();

  RefPtr<IDBRequest> AddRequest(RefPtr<IDBRequest> request);

  /**
   * Not to be confused with the JavaScript commit() method, this synchronously
   * runs all the pending requests and executes the transaction in the given
   * sqlite connection.
   */
  void DoCommit(SqliteConnection* connection);
  void DoCommit(SqliteTransaction* transaction);

  void AddObjectStore(const std::string& name);
  void DeleteObjectStore(const std::string& name);

  Listener on_abort;
  Listener on_complete;
  Listener on_error;

  const Member<IDBDatabase> db;
  Member<dom::DOMException> error;
  const IDBTransactionMode mode;
  bool aborted;
  bool active;
  bool done;

  // If this is a version change transaction, this is the stored transaction
  // object.  This is only valid while the transaction is "active".
  SqliteTransaction* sqlite_transaction;

 private:
  // This must be a list to ensure existing iterators aren't invalidated when
  // inserting.
  std::list<Member<IDBRequest>> requests_;

  std::unordered_map<std::string, Member<IDBObjectStore>> scope_;
};

class IDBTransactionFactory
    : public BackingObjectFactory<IDBTransaction, events::EventTarget> {
 public:
  IDBTransactionFactory();
};

}  // namespace idb
}  // namespace js
}  // namespace shaka

DEFINE_ENUM_MAPPING(shaka::js::idb, IDBTransactionMode) {
  AddMapping(Enum::READ_ONLY, "readonly");
  AddMapping(Enum::READ_WRITE, "readwrite");
  AddMapping(Enum::VERSION_CHANGE, "versionchange");
}

#endif  // SHAKA_EMBEDDED_JS_IDB_TRANSACTION_H_
