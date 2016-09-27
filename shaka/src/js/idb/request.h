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

#ifndef SHAKA_EMBEDDED_JS_IDB_REQUEST_H_
#define SHAKA_EMBEDDED_JS_IDB_REQUEST_H_

#include "shaka/optional.h"
#include "shaka/variant.h"
#include "src/core/member.h"
#include "src/core/ref_ptr.h"
#include "src/js/events/event_target.h"
#include "src/js/js_error.h"
#include "src/mapping/any.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/exception_or.h"

namespace shaka {
namespace js {

namespace dom {
class DOMException;
}  // namespace dom

namespace idb {

class IDBCursor;
class IDBObjectStore;
class IDBTransaction;
class SqliteTransaction;

enum class IDBRequestReadyState {
  PENDING,
  DONE,
};

class IDBRequest : public events::EventTarget {
  DECLARE_TYPE_INFO(IDBRequest);

 public:
  IDBRequest();

  void Trace(memory::HeapTracer* tracer) const override;

  /**
   * Performs the operation for this request.  This will synchronously fire
   * events into JavaScript.
   */
  virtual void PerformOperation(SqliteTransaction* transaction) = 0;

  /**
   * Called if the request is part of a transaction that gets aborted.  This
   * synchronously fires the error event.
   */
  void OnAbort();

  ExceptionOr<Any> Result() const;
  ExceptionOr<Any> Error() const;

  optional<variant<Member<IDBObjectStore>, Member<IDBCursor>>> source;
  Member<IDBTransaction> transaction;
  IDBRequestReadyState ready_state = IDBRequestReadyState::PENDING;

  Listener on_success;
  Listener on_error;

 protected:
  /**
   * Called when the request is completed with a success.  This synchronously
   * invokes the success callback.
   */
  void CompleteSuccess(Any result);

  /**
   * Called when the request is completed with an error.  This synchronously
   * invokes the error callback.
   */
  void CompleteError(JsError error);

  Any result_;
  Any error_;
};

class IDBRequestFactory
    : public BackingObjectFactory<IDBRequest, events::EventTarget> {
 public:
  IDBRequestFactory();
};

}  // namespace idb
}  // namespace js
}  // namespace shaka

DEFINE_ENUM_MAPPING(shaka::js::idb, IDBRequestReadyState) {
  AddMapping(Enum::PENDING, "pending");
  AddMapping(Enum::DONE, "done");
}

#endif  // SHAKA_EMBEDDED_JS_IDB_REQUEST_H_
