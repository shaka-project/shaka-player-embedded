// Copyright 2019 Google LLC
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

#ifndef SHAKA_EMBEDDED_JS_IDB_DELETE_DB_REQUEST_H_
#define SHAKA_EMBEDDED_JS_IDB_DELETE_DB_REQUEST_H_

#include <string>

#include "src/js/idb/request.h"

namespace shaka {
namespace js {
namespace idb {

class IDBDeleteDBRequest : public IDBRequest {
 public:
  explicit IDBDeleteDBRequest(const std::string& name);

  void DoOperation(const std::string& db_path);

  void PerformOperation(SqliteTransaction* transaction) override;

 private:
  const std::string name_;
};

}  // namespace idb
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_IDB_DELETE_DB_REQUEST_H_
