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

#ifndef SHAKA_EMBEDDED_JS_IDB_CURSOR_H_
#define SHAKA_EMBEDDED_JS_IDB_CURSOR_H_

#include "shaka/optional.h"
#include "src/core/member.h"
#include "src/js/idb/idb_utils.h"
#include "src/mapping/any.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/enum.h"
#include "src/mapping/exception_or.h"

namespace shaka {
namespace js {
namespace idb {

class IDBObjectStore;
class IDBRequest;
class IDBIterateCursorRequest;

enum class IDBCursorDirection {
  NEXT,
  NEXT_UNIQUE,
  PREV,
  PREV_UNIQUE,
};

class IDBCursor : public BackingObject {
  DECLARE_TYPE_INFO(IDBCursor);

 public:
  IDBCursor(RefPtr<IDBObjectStore> source, IDBCursorDirection dir);

  void Trace(memory::HeapTracer* tracer) const override;

  const Member<IDBObjectStore> source;
  Member<IDBIterateCursorRequest> request;
  const IDBCursorDirection direction;
  // This is the key of the current record (aka position).  We only iterate on
  // object stores, so this is also the "effective key" and the primaryKey
  // property.
  optional<IdbKeyType> key;
  Any value;
  bool got_value = false;

  ExceptionOr<void> Continue(optional<Any> key);
  ExceptionOr<RefPtr<IDBRequest>> Delete();
};

class IDBCursorFactory : public BackingObjectFactory<IDBCursor> {
 public:
  IDBCursorFactory();
};

}  // namespace idb
}  // namespace js
}  // namespace shaka

DEFINE_ENUM_MAPPING(shaka::js::idb, IDBCursorDirection) {
  AddMapping(Enum::NEXT, "next");
  AddMapping(Enum::NEXT_UNIQUE, "nextunique");
  AddMapping(Enum::PREV, "prev");
  AddMapping(Enum::PREV_UNIQUE, "prevunique");
}

#endif  // SHAKA_EMBEDDED_JS_IDB_CURSOR_H_
