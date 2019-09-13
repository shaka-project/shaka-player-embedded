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

#include "src/js/idb/sqlite.h"

#include <sqlite3.h>

#include <functional>
#include <memory>
#include <utility>

#include "src/util/utils.h"

namespace shaka {
namespace js {
namespace idb {

namespace {

#define RETURN_IF_ERROR(code)           \
  do {                                  \
    const DatabaseStatus ret = (code);  \
    if (ret != DatabaseStatus::Success) \
      return ret;                       \
  } while (false)

DatabaseStatus MapErrorCode(int ret) {
  // See https://www.sqlite.org/rescode.html
  switch (ret & 0xff) {
    case SQLITE_DONE:
    case SQLITE_OK:
      return DatabaseStatus::Success;
    case SQLITE_BUSY:
    case SQLITE_LOCKED:
      VLOG(2) << "Sqlite database busy";
      return DatabaseStatus::Busy;
    // We use the EMPTY key for when we expect a single value and none are
    // returned.  It is unused within sqlite.
    case SQLITE_EMPTY:
      VLOG(2) << "No entries returned";
      return DatabaseStatus::NotFound;

    default:
      if (ret == SQLITE_CONSTRAINT_FOREIGNKEY) {
        VLOG(2) << "Foreign key not found";
        return DatabaseStatus::NotFound;
      } else if (ret == SQLITE_CONSTRAINT_PRIMARYKEY ||
                 ret == SQLITE_CONSTRAINT_UNIQUE) {
        VLOG(2) << "Duplicate entries in table";
        return DatabaseStatus::AlreadyExists;
      }

      LOG(DFATAL) << "Unknown error from sqlite (" << ret
                  << "): " << sqlite3_errstr(ret);
      return DatabaseStatus::UnknownError;
  }
}

template <typename T>
struct GetColumn;
template <>
struct GetColumn<std::string> {
  static std::string Get(sqlite3_stmt* stmt, size_t index) {
    const size_t size = sqlite3_column_bytes(stmt, index);
    auto* ret = sqlite3_column_text(stmt, index);
    return std::string{reinterpret_cast<const char*>(ret), size};
  }
};
template <>
struct GetColumn<std::vector<uint8_t>> {
  static std::vector<uint8_t> Get(sqlite3_stmt* stmt, size_t index) {
    const size_t size = sqlite3_column_bytes(stmt, index);
    auto* ret =
        reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, index));
    return std::vector<uint8_t>{ret, ret + size};
  }
};
template <>
struct GetColumn<int64_t> {
  static int64_t Get(sqlite3_stmt* stmt, size_t index) {
    return sqlite3_column_int64(stmt, index);
  }
};

template <typename Func, typename... Columns>
struct GetColumns;
template <typename Func>
struct GetColumns<Func> {
  template <typename... Args>
  static int Get(sqlite3_stmt* /* stmt */, size_t /* index */, Func&& func,
                 Args&&... args) {
    return func(std::forward<Args>(args)...);
  }
};
template <typename Func, typename T, typename... Rest>
struct GetColumns<Func, T, Rest...> {
  template <typename... Args>
  static int Get(sqlite3_stmt* stmt, size_t index, Func&& func,
                 Args&&... args) {
    T temp = GetColumn<T>::Get(stmt, index);
    return GetColumns<Func, Rest...>::Get(
        stmt, index + 1, std::forward<Func>(func), std::forward<Args>(args)...,
        std::move(temp));
  }
};

template <typename T>
struct BindSingleArg;
template <>
struct BindSingleArg<std::string> {
  static int Bind(sqlite3_stmt* stmt, size_t index, const std::string& arg) {
    return sqlite3_bind_text(stmt, index, arg.c_str(), arg.size(),
                             SQLITE_TRANSIENT);  // NOLINT
  }
};
template <>
struct BindSingleArg<std::vector<uint8_t>> {
  static int Bind(sqlite3_stmt* stmt, size_t index,
                  const std::vector<uint8_t>& arg) {
    return sqlite3_bind_blob64(stmt, index, arg.data(), arg.size(),
                               SQLITE_TRANSIENT);  // NOLINT
  }
};
template <>
struct BindSingleArg<int64_t> {
  static int Bind(sqlite3_stmt* stmt, size_t index, int64_t arg) {
    return sqlite3_bind_int64(stmt, index, arg);
  }
};

template <typename... Args>
struct BindArgs;
template <>
struct BindArgs<> {
  static int Bind(sqlite3_stmt* /* stmt */, size_t /* offset */) {
    return SQLITE_OK;
  }
};
template <typename T, typename... Rest>
struct BindArgs<T, Rest...> {
  static int Bind(sqlite3_stmt* stmt, size_t offset, T&& cur, Rest&&... rest) {
    const int ret = BindSingleArg<typename std::decay<T>::type>::Bind(
        stmt, offset, std::forward<T>(cur));
    if (ret != SQLITE_OK)
      return ret;
    return BindArgs<Rest...>::Bind(stmt, offset + 1,
                                   std::forward<Rest>(rest)...);
  }
};


template <typename... InParams, typename... Columns>
DatabaseStatus ExecGetResults(sqlite3* db, std::function<int(Columns...)> cb,
                              const std::string& cmd, InParams&&... params) {
  VLOG(2) << "Querying sqlite: " << cmd;

  sqlite3_stmt* stmt;
  int ret = sqlite3_prepare_v2(db, cmd.c_str(), cmd.size(), &stmt, nullptr);
  if (ret != SQLITE_OK)
    return MapErrorCode(ret);
  std::unique_ptr<sqlite3_stmt, int (*)(sqlite3_stmt*)> stmt_safe(
      stmt, &sqlite3_finalize);

  ret = BindArgs<InParams...>::Bind(stmt, 1, std::forward<InParams>(params)...);
  if (ret != SQLITE_OK)
    return MapErrorCode(ret);

  while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
    ret = GetColumns<std::function<int(Columns...)>&, Columns...>::Get(stmt, 0,
                                                                       cb);
    if (ret != SQLITE_OK)
      return MapErrorCode(ret);
  }
  return MapErrorCode(ret == SQLITE_DONE ? SQLITE_OK : ret);
}

template <typename... Args>
DatabaseStatus ExecCommand(sqlite3* db, const std::string& cmd,
                           Args&&... args) {
  std::function<int()> ignore = []() { return SQLITE_OK; };
  return ExecGetResults(db, ignore, cmd, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
DatabaseStatus ExecGetSingleResult(sqlite3* db, T* result,
                                   const std::string& cmd, Args&&... args) {
  bool got = false;
  std::function<int(T)> get = [&](T value) {
    if (got)
      return SQLITE_ERROR;

    using std::swap;
    swap(value, *result);

    got = true;
    return SQLITE_OK;
  };
  RETURN_IF_ERROR(ExecGetResults(db, get, cmd, std::forward<Args>(args)...));
  return got ? DatabaseStatus::Success : DatabaseStatus::NotFound;
}

}  // namespace


SqliteTransaction::SqliteTransaction() : db_(nullptr) {}
SqliteTransaction::SqliteTransaction(SqliteTransaction&& other)
    : db_(other.db_) {
  other.db_ = nullptr;
}
SqliteTransaction::~SqliteTransaction() {
  if (db_) {
    Rollback();
  }
}

SqliteTransaction& SqliteTransaction::operator=(SqliteTransaction&& other) {
  if (db_) {
    Rollback();
  }
  db_ = other.db_;
  other.db_ = nullptr;
  return *this;
}


DatabaseStatus SqliteTransaction::CreateDb(const std::string& db_name,
                                           int64_t version) {
  DCHECK(db_) << "Transaction is closed";
  if (version <= 0)
    return DatabaseStatus::BadVersionNumber;

  const std::string cmd =
      "INSERT INTO databases (name, version) VALUES (?1, ?2)";
  return ExecCommand(db_, cmd, db_name, version);
}

DatabaseStatus SqliteTransaction::UpdateDbVersion(const std::string& db_name,
                                                  int64_t version) {
  DCHECK(db_) << "Transaction is closed";
  int64_t old_version;
  RETURN_IF_ERROR(GetDbVersion(db_name, &old_version));
  if (version <= old_version)
    return DatabaseStatus::BadVersionNumber;

  const std::string cmd = "UPDATE databases SET version = ?2 WHERE name == ?1";
  return ExecCommand(db_, cmd, db_name, version);
}

DatabaseStatus SqliteTransaction::DeleteDb(const std::string& db_name) {
  DCHECK(db_) << "Transaction is closed";
  // Check that it exists first so we can throw a not found error.
  int64_t version;
  RETURN_IF_ERROR(GetDbVersion(db_name, &version));

  // Because of the "ON CASCADE" on the table, we don't need to explicitly
  // delete the stores or the data entries.
  const std::string delete_cmd = "DELETE FROM databases WHERE name == ?1";
  return ExecCommand(db_, delete_cmd, db_name);
}

DatabaseStatus SqliteTransaction::GetDbVersion(const std::string& db_name,
                                               int64_t* version) {
  DCHECK(db_) << "Transaction is closed";
  const std::string cmd = "SELECT version FROM databases WHERE name == ?1";
  return ExecGetSingleResult(db_, version, cmd, db_name);
}


DatabaseStatus SqliteTransaction::CreateObjectStore(
    const std::string& db_name, const std::string& store_name) {
  DCHECK(db_) << "Transaction is closed";
  const std::string cmd =
      "INSERT INTO object_stores (db_name, store_name) VALUES (?1, ?2)";
  // If the database doesn't exist, we'll get a foreign key error.
  // If there is a store with the same name already, we'll get a primary key
  // error.
  return ExecCommand(db_, cmd, db_name, store_name);
}

DatabaseStatus SqliteTransaction::DeleteObjectStore(
    const std::string& db_name, const std::string& store_name) {
  DCHECK(db_) << "Transaction is closed";
  // Check that it exists first so we can throw a not found error.
  int64_t store_id;
  RETURN_IF_ERROR(GetStoreId(db_name, store_name, &store_id));

  // Because of the "ON CASCADE" on the table, we don't need to explicitly
  // delete the data entries.
  const std::string cmd =
      "DELETE FROM object_stores WHERE db_name == ?1 AND store_name == ?2";
  return ExecCommand(db_, cmd, db_name, store_name);
}

DatabaseStatus SqliteTransaction::ListObjectStores(
    const std::string& db_name, std::vector<std::string>* names) {
  DCHECK(db_) << "Transaction is closed";
  DCHECK(names);

  // Check that it exists first so we can throw a not found error.
  int64_t version;
  RETURN_IF_ERROR(GetDbVersion(db_name, &version));

  std::function<int(std::string)> cb = [&](std::string value) {
    names->push_back(std::move(value));
    return SQLITE_OK;
  };
  const std::string cmd =
      "SELECT store_name FROM object_stores WHERE db_name == ?1";
  return ExecGetResults(db_, cb, cmd, db_name);
}


DatabaseStatus SqliteTransaction::AddData(const std::string& db_name,
                                          const std::string& store_name,
                                          const std::vector<uint8_t>& data,
                                          int64_t* key) {
  DCHECK(db_) << "Transaction is closed";
  int64_t store_id;
  RETURN_IF_ERROR(GetStoreId(db_name, store_name, &store_id));

  const std::string select_cmd =
      "SELECT COALESCE(MAX(key), 0) FROM objects WHERE store == ?1";
  RETURN_IF_ERROR(ExecGetSingleResult(db_, key, select_cmd, store_id));
  (*key)++;

  const std::string insert_cmd =
      "INSERT INTO objects (store, key, body) VALUES (?1, ?2, ?3)";
  return ExecCommand(db_, insert_cmd, store_id, *key, data);
}

DatabaseStatus SqliteTransaction::GetData(const std::string& db_name,
                                          const std::string& store_name,
                                          int64_t key,
                                          std::vector<uint8_t>* data) {
  DCHECK(db_) << "Transaction is closed";
  const std::string cmd =
      "SELECT body FROM objects "
      "INNER JOIN object_stores ON object_stores.id == objects.store "
      "WHERE db_name == ?1 AND store_name == ?2 AND key == ?3";
  return ExecGetSingleResult(db_, data, cmd, db_name, store_name, key);
}

DatabaseStatus SqliteTransaction::UpdateData(const std::string& db_name,
                                             const std::string& store_name,
                                             int64_t key,
                                             const std::vector<uint8_t>& data) {
  DCHECK(db_) << "Transaction is closed";
  int64_t store_id;
  RETURN_IF_ERROR(GetStoreId(db_name, store_name, &store_id));

  const std::string cmd =
      "INSERT OR REPLACE INTO objects (store, key, body) VALUES (?1, ?2, ?3)";
  return ExecCommand(db_, cmd, store_id, key, data);
}

DatabaseStatus SqliteTransaction::DeleteData(const std::string& db_name,
                                             const std::string& store_name,
                                             int64_t key) {
  DCHECK(db_) << "Transaction is closed";
  const std::string cmd = R"(
      DELETE FROM objects WHERE key == ?3 AND store == (
          SELECT id FROM object_stores WHERE db_name == ?1 AND store_name == ?2)
  )";
  return ExecCommand(db_, cmd, db_name, store_name, key);
}

DatabaseStatus SqliteTransaction::FindData(const std::string& db_name,
                                           const std::string& store_name,
                                           optional<int64_t> key,
                                           bool ascending, int64_t* found_key) {
  DCHECK(db_) << "Transaction is closed";
  std::string cmd;
  // Use StringPrintf for part of this since Sqlite parameters can't introduce
  // syntax, they are just for expressions.
  if (!key.has_value()) {
    cmd = util::StringPrintf(
        R"(
            SELECT key FROM objects
            WHERE store == (SELECT id FROM object_stores
                            WHERE db_name == ?1 AND store_name == ?2)
            ORDER BY key %s
            LIMIT 1
        )",
        ascending ? "ASC" : "DESC");
    return ExecGetSingleResult(db_, found_key, cmd, db_name, store_name);
  }
  cmd = util::StringPrintf(
      R"(
          SELECT key FROM objects
          WHERE store == (SELECT id FROM object_stores
                          WHERE db_name == ?1 AND store_name == ?2) AND
                key %s ?3
          ORDER BY key %s
          LIMIT 1
      )",
      ascending ? ">" : "<", ascending ? "ASC" : "DESC");
  return ExecGetSingleResult(db_, found_key, cmd, db_name, store_name,
                             key.value());
}


DatabaseStatus SqliteTransaction::Commit() {
  DCHECK(db_) << "Transaction is closed";
  auto* db = db_;
  db_ = nullptr;
  return ExecCommand(db, "COMMIT");
}

DatabaseStatus SqliteTransaction::Rollback() {
  DCHECK(db_) << "Transaction is closed";
  auto* db = db_;
  db_ = nullptr;
  return ExecCommand(db, "ROLLBACK");
}


DatabaseStatus SqliteTransaction::GetStoreId(const std::string& db_name,
                                             const std::string& store_name,
                                             int64_t* store_id) {
  const std::string get_cmd =
      "SELECT id FROM object_stores "
      "WHERE db_name == ?1 AND store_name == ?2";
  return ExecGetSingleResult(db_, store_id, get_cmd, db_name, store_name);
}


SqliteConnection::SqliteConnection(const std::string& file_path)
    : path_(file_path), db_(nullptr) {}
SqliteConnection::~SqliteConnection() {
  if (db_) {
    const auto ret = sqlite3_close(db_);
    if (ret != SQLITE_OK) {
      LOG(ERROR) << "Error closing sqlite connection: " << sqlite3_errstr(ret);
    }
  }
}

DatabaseStatus SqliteConnection::Init() {
  sqlite3* db;
  RETURN_IF_ERROR(MapErrorCode(sqlite3_open(path_.c_str(), &db)));
  db_ = db;

  // Enable extended error codes.
  RETURN_IF_ERROR(MapErrorCode(sqlite3_extended_result_codes(db, 1)));

  const std::string init_cmd = R"(
      -- Timeout, in milliseconds, to wait if there is an exclusive lock on the
      -- database.  When in WAL mode, we can have non-exclusive writes, so we
      -- should never get busy normally.
      PRAGMA busy_timeout = 250;
      PRAGMA foreign_keys = ON;
      -- Switch to WAL journaling mode; this is faster (usually) and allows for
      -- non-exclusive write transactions.
      PRAGMA journal_mode = WAL;

      CREATE TABLE IF NOT EXISTS databases (
        name TEXT NOT NULL PRIMARY KEY,
        version INTEGER NOT NULL,
        CHECK (version > 0)
      ) WITHOUT ROWID;

      CREATE TABLE IF NOT EXISTS object_stores (
        id INTEGER PRIMARY KEY NOT NULL,
        db_name TEXT NOT NULL,
        store_name TEXT NOT NULL,
        UNIQUE (db_name, store_name),
        FOREIGN KEY (db_name) REFERENCES databases(name) ON DELETE CASCADE
      );

      CREATE TABLE IF NOT EXISTS objects (
        store INTEGER NOT NULL,
        key INTEGER NOT NULL,
        body BLOB NOT NULL,
        PRIMARY KEY (store, key),
        FOREIGN KEY (store) REFERENCES object_stores (id) ON DELETE CASCADE
      ) WITHOUT ROWID;
  )";
  RETURN_IF_ERROR(MapErrorCode(
      sqlite3_exec(db, init_cmd.c_str(), nullptr, nullptr, nullptr)));

  return DatabaseStatus::Success;
}

DatabaseStatus SqliteConnection::BeginTransaction(
    SqliteTransaction* transaction) {
  RETURN_IF_ERROR(ExecCommand(db_, "BEGIN TRANSACTION"));
  transaction->db_ = db_;
  return DatabaseStatus::Success;
}

DatabaseStatus SqliteConnection::Flush() {
  return MapErrorCode(sqlite3_wal_checkpoint_v2(
      db_, nullptr, SQLITE_CHECKPOINT_PASSIVE, nullptr, nullptr));
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
