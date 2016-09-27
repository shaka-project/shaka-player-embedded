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

#ifndef SHAKA_EMBEDDED_JS_IDB_OBJECT_STORE_H_
#define SHAKA_EMBEDDED_JS_IDB_OBJECT_STORE_H_

#include <string>

#include "shaka/optional.h"
#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/js/idb/cursor.h"
#include "src/js/idb/idb_utils.h"
#include "src/mapping/any.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/exception_or.h"

namespace shaka {
namespace js {
namespace idb {

class IDBRequest;
class IDBTransaction;

class IDBObjectStore : public BackingObject {
  DECLARE_TYPE_INFO(IDBObjectStore);

 public:
  IDBObjectStore();

  void Trace(memory::HeapTracer* tracer) const override;

  const bool auto_increment = true;
  const optional<std::string> key_path;
  const std::string store_name;  // JavaScript "name"
  const Member<IDBTransaction> transaction;

  ExceptionOr<RefPtr<IDBRequest>> Add(Any value, optional<IdbKeyType> key);
  ExceptionOr<RefPtr<IDBRequest>> Put(Any value, optional<IdbKeyType> key);
  ExceptionOr<RefPtr<IDBRequest>> Delete(IdbKeyType key);
  ExceptionOr<RefPtr<IDBRequest>> Get(IdbKeyType key);
  ExceptionOr<RefPtr<IDBRequest>> OpenCursor(
      optional<IdbKeyType> range, optional<IDBCursorDirection> direction);

  ExceptionOr<RefPtr<IDBRequest>> AddOrPut(Any value, optional<IdbKeyType> key,
                                           bool no_overwrite);
};

class IDBObjectStoreFactory : public BackingObjectFactory<IDBObjectStore> {
 public:
  IDBObjectStoreFactory();
};

}  // namespace idb
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_IDB_OBJECT_STORE_H_
