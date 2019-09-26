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

#include "src/js/idb/object_store.h"
#include "src/js/idb/transaction.h"
#include "src/js/js_error.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace idb {

IDBDatabase::IDBDatabase() : db_name(""), version(0) {
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
}

ExceptionOr<RefPtr<IDBObjectStore>> IDBDatabase::CreateObjectStore(
    const std::string& /* name */,
    optional<IDBObjectStoreParameters> /* parameters */) {
  return JsError::DOMException(NotSupportedError);
}

ExceptionOr<void> IDBDatabase::DeleteObjectStore(
    const std::string& /* name */) {
  return JsError::DOMException(NotSupportedError);
}

ExceptionOr<RefPtr<IDBTransaction>> IDBDatabase::Transaction(
    variant<std::string, std::vector<std::string>> /* store_names */,
    optional<IDBTransactionMode> /* mode */) {
  return JsError::DOMException(NotSupportedError);
}

void IDBDatabase::Close() {}


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
