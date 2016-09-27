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

#include "src/js/idb/request.h"
#include "src/js/idb/transaction.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace idb {

IDBObjectStore::IDBObjectStore() {}
IDBObjectStore::~IDBObjectStore() {}

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
    Any /* value */, optional<IdbKeyType> /* key */, bool /* no_overwrite */) {
  return JsError::DOMException(NotSupportedError);
}

ExceptionOr<RefPtr<IDBRequest>> IDBObjectStore::Delete(IdbKeyType /* key */) {
  return JsError::DOMException(NotSupportedError);
}

ExceptionOr<RefPtr<IDBRequest>> IDBObjectStore::Get(IdbKeyType /* key */) {
  return JsError::DOMException(NotSupportedError);
}

ExceptionOr<RefPtr<IDBRequest>> IDBObjectStore::OpenCursor(
    optional<IdbKeyType> /* range */,
    optional<IDBCursorDirection> /* direction */) {
  return JsError::DOMException(NotSupportedError);
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
