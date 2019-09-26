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

#include "src/js/idb/open_db_request.h"
#include "src/js/idb/request.h"

namespace shaka {
namespace js {
namespace idb {

IDBFactory::IDBFactory() {}
// \cond Doxygen_Skip
IDBFactory::~IDBFactory() {}
// \endcond Doxygen_Skip

RefPtr<IDBOpenDBRequest> IDBFactory::Open(const std::string& /* name */,
                                          optional<uint32_t> /* version */) {
  return nullptr;
}

RefPtr<IDBRequest> IDBFactory::DeleteDatabase(const std::string& /* name */) {
  return nullptr;
}


IDBFactoryFactory::IDBFactoryFactory() {
  AddMemberFunction("open", &IDBFactory::Open);
  AddMemberFunction("deleteDatabase", &IDBFactory::DeleteDatabase);
  NotImplemented("cmp");
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
