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

#include "src/js/idb/transaction.h"

#include "src/js/dom/dom_exception.h"
#include "src/js/idb/database.h"
#include "src/js/idb/object_store.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace idb {

IDBTransaction::IDBTransaction()
    : db(nullptr), error(nullptr), mode(IDBTransactionMode::READ_ONLY) {
  AddListenerField(EventType::Abort, &on_abort);
  AddListenerField(EventType::Complete, &on_complete);
  AddListenerField(EventType::Error, &on_error);
}

// \cond Doxygen_Skip
IDBTransaction::~IDBTransaction() {}
// \endcond Doxygen_Skip

void IDBTransaction::Trace(memory::HeapTracer* tracer) const {
  events::EventTarget::Trace(tracer);
  tracer->Trace(&db);
  tracer->Trace(&error);
}

ExceptionOr<RefPtr<IDBObjectStore>> IDBTransaction::ObjectStore(
    const std::string /* name */) {
  return JsError::DOMException(NotSupportedError);
}

ExceptionOr<void> IDBTransaction::Abort() {
  return JsError::DOMException(NotSupportedError);
}


IDBTransactionFactory::IDBTransactionFactory() {
  AddReadOnlyProperty("mode", &IDBTransaction::mode);
  AddReadOnlyProperty("db", &IDBTransaction::db);
  AddReadOnlyProperty("error", &IDBTransaction::error);

  AddListenerField(EventType::Abort, &IDBTransaction::on_abort);
  AddListenerField(EventType::Complete, &IDBTransaction::on_complete);
  AddListenerField(EventType::Error, &IDBTransaction::on_error);

  AddMemberFunction("objectStore", &IDBTransaction::ObjectStore);
  AddMemberFunction("abort", &IDBTransaction::Abort);

  NotImplemented("commit");
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
