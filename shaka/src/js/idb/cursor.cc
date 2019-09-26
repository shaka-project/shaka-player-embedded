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

#include "src/js/idb/cursor.h"

#include "src/js/idb/object_store.h"
#include "src/js/idb/request.h"
#include "src/js/idb/transaction.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace idb {

IDBCursor::IDBCursor(RefPtr<IDBObjectStore> source, IDBCursorDirection dir)
    : source(source), direction(dir) {}
// \cond Doxygen_Skip
IDBCursor::~IDBCursor() {}
// \endcond Doxygen_Skip

void IDBCursor::Trace(memory::HeapTracer* tracer) const {
  BackingObject::Trace(tracer);
  tracer->Trace(&source);
  tracer->Trace(&request);
  tracer->Trace(&value);
}

ExceptionOr<void> IDBCursor::Continue(optional<Any> /* key */) {
  return {};
}

ExceptionOr<RefPtr<IDBRequest>> IDBCursor::Delete() {
  return nullptr;
}


IDBCursorFactory::IDBCursorFactory() {
  AddReadOnlyProperty("direction", &IDBCursor::direction);
  AddReadOnlyProperty("key", &IDBCursor::key);
  AddReadOnlyProperty("primaryKey", &IDBCursor::key);
  AddReadOnlyProperty("request", &IDBCursor::request);
  AddReadOnlyProperty("source", &IDBCursor::source);
  AddReadOnlyProperty("value", &IDBCursor::value);

  AddMemberFunction("continue", &IDBCursor::Continue);
  AddMemberFunction("delete", &IDBCursor::Delete);

  NotImplemented("advance");
  NotImplemented("update");
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
