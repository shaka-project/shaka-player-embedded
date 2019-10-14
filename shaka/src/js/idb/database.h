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

#ifndef SHAKA_EMBEDDED_JS_IDB_DATABASE_H_
#define SHAKA_EMBEDDED_JS_IDB_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "shaka/optional.h"
#include "shaka/variant.h"
#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/js/dom/dom_string_list.h"
#include "src/js/events/event_target.h"
#include "src/js/idb/sqlite.h"
#include "src/js/idb/transaction.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/exception_or.h"
#include "src/mapping/struct.h"

namespace shaka {
namespace js {
namespace idb {

class IDBObjectStore;

struct IDBObjectStoreParameters : Struct {
  DECLARE_STRUCT_SPECIAL_METHODS_COPYABLE(IDBObjectStoreParameters);

  ADD_DICT_FIELD(keyPath, std::string);
  ADD_DICT_FIELD(autoIncrement, bool);
};

class IDBDatabase : public events::EventTarget {
  DECLARE_TYPE_INFO(IDBDatabase);

 public:
  IDBDatabase(std::shared_ptr<SqliteConnection> connection,
              const std::string& name, int64_t version,
              const std::vector<std::string>& store_names);

  void Trace(memory::HeapTracer* tracer) const override;

  Listener on_abort;
  Listener on_error;
  Listener on_version_change;

  bool is_closed() const {
    return close_pending_;
  }

  const std::string db_name;  // JavaScript "name"
  Member<dom::DOMStringList> object_store_names;
  const int64_t version;

  ExceptionOr<RefPtr<IDBObjectStore>> CreateObjectStore(
      const std::string& name, optional<IDBObjectStoreParameters> parameters);
  ExceptionOr<void> DeleteObjectStore(const std::string& name);

  ExceptionOr<RefPtr<IDBTransaction>> Transaction(
      variant<std::string, std::vector<std::string>> store_names,
      optional<IDBTransactionMode> mode);
  void Close();

  void VersionChangeTransaction(RefPtr<IDBTransaction> trans) {
    version_change_trans_ = trans;
  }

 private:
  Member<IDBTransaction> version_change_trans_;
  std::shared_ptr<SqliteConnection> connection_;
  bool close_pending_ = false;
};

class IDBDatabaseFactory
    : public BackingObjectFactory<IDBDatabase, events::EventTarget> {
 public:
  IDBDatabaseFactory();
};

}  // namespace idb
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_IDB_DATABASE_H_
