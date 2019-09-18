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

#include "src/js/idb/idb_factory.h"

#include <utility>

#include "src/core/js_manager_impl.h"
#include "src/js/idb/delete_db_request.h"
#include "src/js/idb/idb_utils.h"
#include "src/js/idb/open_db_request.h"
#include "src/js/idb/request.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace idb {

namespace {

constexpr const char* kDbFileName = "shaka_indexeddb.db";

template <typename T>
class Commit : public memory::Traceable {
 public:
  Commit(RefPtr<T> request, const std::string& db_path)
      : request_(request), path_(db_path) {}

  void Trace(memory::HeapTracer* tracer) const override {
    tracer->Trace(&request_);
  }

  void operator()() {
    request_->DoOperation(path_);
  }

 private:
  const Member<T> request_;
  const std::string path_;
};

}  // namespace

IDBFactory::IDBFactory() {}
// \cond Doxygen_Skip
IDBFactory::~IDBFactory() {}
// \endcond Doxygen_Skip

RefPtr<IDBOpenDBRequest> IDBFactory::Open(const std::string& name,
                                          optional<uint64_t> version) {
  RefPtr<IDBOpenDBRequest> request(new IDBOpenDBRequest(name, version));
  const std::string db_path =
      JsManagerImpl::Instance()->GetPathForDynamicFile(kDbFileName);
  JsManagerImpl::Instance()->MainThread()->AddInternalTask(
      TaskPriority::Internal, "IndexedDb::open",
      Commit<IDBOpenDBRequest>(request, db_path));
  return request;
}

RefPtr<IDBOpenDBRequest> IDBFactory::OpenTestDb() {
  RefPtr<IDBOpenDBRequest> request(new IDBOpenDBRequest("test", 1));
  // Use a temporary database name.  This will be cleaned up by sqlite.
  JsManagerImpl::Instance()->MainThread()->AddInternalTask(
      TaskPriority::Internal, "IndexedDb::openTestDb",
      Commit<IDBOpenDBRequest>(request, ""));
  return request;
}

RefPtr<IDBRequest> IDBFactory::DeleteDatabase(const std::string& name) {
  RefPtr<IDBDeleteDBRequest> request(new IDBDeleteDBRequest(name));
  const std::string db_path =
      JsManagerImpl::Instance()->GetPathForDynamicFile(kDbFileName);
  JsManagerImpl::Instance()->MainThread()->AddInternalTask(
      TaskPriority::Internal, "IndexedDb::deleteDatabase",
      Commit<IDBDeleteDBRequest>(request, db_path));
  return request;
}

ExceptionOr<Any> IDBFactory::CloneForTesting(Any value) {
  proto::Value temp;
  RETURN_IF_ERROR(StoreInProto(value, &temp));
  return LoadFromProto(temp);
}


IDBFactoryFactory::IDBFactoryFactory() {
  AddMemberFunction("cloneForTesting", &IDBFactory::CloneForTesting);
  AddMemberFunction("open", &IDBFactory::Open);
  AddMemberFunction("openTestDb", &IDBFactory::OpenTestDb);
  AddMemberFunction("deleteDatabase", &IDBFactory::DeleteDatabase);

  NotImplemented("cmp");
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
