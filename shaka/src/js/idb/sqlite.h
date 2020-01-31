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

#ifndef SHAKA_EMBEDDED_JS_IDB_SQLITE_H
#define SHAKA_EMBEDDED_JS_IDB_SQLITE_H

#include <atomic>
#include <string>
#include <vector>

#include "shaka/optional.h"
#include "src/util/macros.h"

struct sqlite3;

namespace shaka {
namespace js {
namespace idb {

enum class DatabaseStatus {
  Success,

  /** The database, object store, or item was not found. */
  NotFound,
  /** An item with the given key/name already exists. */
  AlreadyExists,
  /** There is another transaction happening (maybe by another program). */
  Busy,
  /**
   * There was an attempt to set a negative version number or change it to a
   * lower value.
   */
  BadVersionNumber,
  UnknownError,
};

/**
 * Represents a single transaction within an sqlite database.  There can only be
 * one transaction alive at one time.  The caller must call Commit or Rollback
 * before this is destroyed.  Once Commit/Rollback is called, the transaction
 * is done and cannot be used further.
 */
class SqliteTransaction {
 public:
  SqliteTransaction();
  SqliteTransaction(SqliteTransaction&&);
  ~SqliteTransaction();

  SHAKA_NON_COPYABLE_TYPE(SqliteTransaction);

  SqliteTransaction& operator=(SqliteTransaction&&);

  bool valid() const {
    return db_;
  }

  DatabaseStatus CreateDb(const std::string& db_name, int64_t version);
  DatabaseStatus UpdateDbVersion(const std::string& db_name, int64_t version);
  DatabaseStatus DeleteDb(const std::string& db_name);
  DatabaseStatus GetDbVersion(const std::string& db_name, int64_t* version);

  DatabaseStatus CreateObjectStore(const std::string& db_name,
                                   const std::string& store_name);
  DatabaseStatus DeleteObjectStore(const std::string& db_name,
                                   const std::string& store_name);
  DatabaseStatus ListObjectStores(const std::string& db_name,
                                  std::vector<std::string>* names);

  /** This will insert a new entry with an auto-generated key. */
  DatabaseStatus AddData(const std::string& db_name,
                         const std::string& store_name,
                         const std::vector<uint8_t>& data, int64_t* key);
  /** Gets the value of the given entry. */
  DatabaseStatus GetData(const std::string& db_name,
                         const std::string& store_name, int64_t key,
                         std::vector<uint8_t>* data);
  /**
   * This will update an existing entry, or create a new one if it doesn't
   * exist.
   */
  DatabaseStatus UpdateData(const std::string& db_name,
                            const std::string& store_name, int64_t key,
                            const std::vector<uint8_t>& data);
  /** Deletes an existing entry.  Does nothing if it doesn't exist. */
  DatabaseStatus DeleteData(const std::string& db_name,
                            const std::string& store_name, int64_t key);
  /** Finds the next/previous data entry from the given key. */
  DatabaseStatus FindData(const std::string& db_name,
                          const std::string& store_name, optional<int64_t> key,
                          bool ascending, int64_t* found_key);

  DatabaseStatus Commit();
  DatabaseStatus Rollback();

 private:
  DatabaseStatus GetStoreId(const std::string& db_name,
                            const std::string& store_name, int64_t* store_id);

  friend class SqliteConnection;
  sqlite3* db_;
};

/**
 * Represents a connection to a sqlite database.  This sets up the connection
 * and ensures the correct tables exist.
 */
class SqliteConnection {
 public:
  /**
   * Creates a new connection to the given database file.
   * @param file_path The path to the database file.  If the file doesn't exist,
   *   it will be created.  If this is the empty string, a temporary database
   *   will be used for testing.
   */
  explicit SqliteConnection(const std::string& file_path);
  ~SqliteConnection();

  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(SqliteConnection);

  /**
   * Initializes the connection and sets up the database as needed.  This MUST
   * be called before calling any other method.
   */
  DatabaseStatus Init();

  /**
   * Begins a new transaction using the given transaction object.  There can
   * only be one transaction happening at once; this will return an error if
   * there is another transaction happening.
   */
  DatabaseStatus BeginTransaction(SqliteTransaction* transaction);


  /**
   * Flushes pending transactions from the journal to the database.  Note this
   * doesn't need to be called and will be handled automatically by the database
   * ending.  Also note a crash will preserve the journal and there will be no
   * data loss.
   *
   * This is called to reduce the size of the journal to make reads faster.
   * This can be called from a background thread to periodically update the
   * journal.  Calling this will not block other transactions from completing.
   */
  DatabaseStatus Flush();

 private:
  const std::string path_;
  // Use an atomic variable so it can be accessed from different threads without
  // a lock.  Sqlite is internally thread-safe.
  std::atomic<sqlite3*> db_;
};

}  // namespace idb
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_IDB_SQLITE_H
