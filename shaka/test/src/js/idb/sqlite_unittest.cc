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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace shaka {
namespace js {
namespace idb {

namespace {

constexpr const char* kDbName = "db";
constexpr const char* kStoreName = "store";

}  // namespace

class SqliteTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_EQ(connection_.Init(), DatabaseStatus::Success);
    ASSERT_EQ(connection_.BeginTransaction(&transaction_),
              DatabaseStatus::Success);
    ASSERT_EQ(transaction_.CreateDb(kDbName, 3), DatabaseStatus::Success);
    ASSERT_EQ(transaction_.CreateObjectStore(kDbName, kStoreName),
              DatabaseStatus::Success);

    ASSERT_EQ(transaction_.AddData(kDbName, kStoreName, {1, 2, 3},
                                   &existing_data_key_),
              DatabaseStatus::Success);
  }

 protected:
  SqliteConnection connection_{""};
  SqliteTransaction transaction_;
  int64_t existing_data_key_;
};

TEST_F(SqliteTest, CreateDb_RejectNegativeVersion) {
  ASSERT_EQ(transaction_.CreateDb("foo", -2), DatabaseStatus::BadVersionNumber);
}

TEST_F(SqliteTest, CreateDb_RejectSameName) {
  ASSERT_EQ(transaction_.CreateDb(kDbName, 10), DatabaseStatus::AlreadyExists);
}

TEST_F(SqliteTest, UpdateDbVersion_Success) {
  int64_t version;
  ASSERT_EQ(transaction_.GetDbVersion(kDbName, &version),
            DatabaseStatus::Success);
  EXPECT_EQ(version, 3);
  ASSERT_EQ(transaction_.UpdateDbVersion(kDbName, 10), DatabaseStatus::Success);
  ASSERT_EQ(transaction_.GetDbVersion(kDbName, &version),
            DatabaseStatus::Success);
  EXPECT_EQ(version, 10);
}

TEST_F(SqliteTest, UpdateDbVersion_CannotLowerVersion) {
  ASSERT_EQ(transaction_.UpdateDbVersion(kDbName, 2),
            DatabaseStatus::BadVersionNumber);
  ASSERT_EQ(transaction_.UpdateDbVersion(kDbName, 0),
            DatabaseStatus::BadVersionNumber);
  ASSERT_EQ(transaction_.UpdateDbVersion(kDbName, -2),
            DatabaseStatus::BadVersionNumber);
}

TEST_F(SqliteTest, UpdateDbVersion_NotFound) {
  ASSERT_EQ(transaction_.UpdateDbVersion("foo", 10), DatabaseStatus::NotFound);
}

TEST_F(SqliteTest, GetDbVersion_NotFound) {
  int64_t version;
  ASSERT_EQ(transaction_.GetDbVersion("foo", &version),
            DatabaseStatus::NotFound);
}

TEST_F(SqliteTest, DeleteDb_NotFound) {
  ASSERT_EQ(transaction_.DeleteDb("foo"), DatabaseStatus::NotFound);
}

TEST_F(SqliteTest, DeleteDb_Success) {
  ASSERT_EQ(transaction_.DeleteDb(kDbName), DatabaseStatus::Success);

  int64_t version;
  EXPECT_EQ(transaction_.GetDbVersion(kDbName, &version),
            DatabaseStatus::NotFound);
  std::vector<std::string> names;
  EXPECT_EQ(transaction_.ListObjectStores(kDbName, &names),
            DatabaseStatus::NotFound);
  std::vector<uint8_t> result;
  EXPECT_EQ(
      transaction_.GetData(kDbName, kStoreName, existing_data_key_, &result),
      DatabaseStatus::NotFound);
}


TEST_F(SqliteTest, CreateObjectStore_UnknownDbName) {
  ASSERT_EQ(transaction_.CreateObjectStore("foo", kStoreName),
            DatabaseStatus::NotFound);
}

TEST_F(SqliteTest, CreateObjectStore_RejectSameName) {
  ASSERT_EQ(transaction_.CreateObjectStore(kDbName, kStoreName),
            DatabaseStatus::AlreadyExists);
}

TEST_F(SqliteTest, DeleteObjectStore_Found) {
  ASSERT_EQ(transaction_.DeleteObjectStore(kDbName, kStoreName),
            DatabaseStatus::Success);
  std::vector<std::string> names;
  ASSERT_EQ(transaction_.ListObjectStores(kDbName, &names),
            DatabaseStatus::Success);
  EXPECT_TRUE(names.empty());
}

TEST_F(SqliteTest, DeleteObjectStore_StoreNameNotFound) {
  ASSERT_EQ(transaction_.DeleteObjectStore(kDbName, "foo"),
            DatabaseStatus::NotFound);
}

TEST_F(SqliteTest, DeleteObjectStore_DbNameNotFound) {
  ASSERT_EQ(transaction_.DeleteObjectStore("foo", kStoreName),
            DatabaseStatus::NotFound);
}

TEST_F(SqliteTest, DeleteObjectStore_DeletesData) {
  ASSERT_EQ(transaction_.DeleteObjectStore(kDbName, kStoreName),
            DatabaseStatus::Success);

  // Create the same store to ensure the data doesn't exist.
  ASSERT_EQ(transaction_.CreateObjectStore(kDbName, kStoreName),
            DatabaseStatus::Success);
  std::vector<uint8_t> result;
  ASSERT_EQ(
      transaction_.GetData(kDbName, kStoreName, existing_data_key_, &result),
      DatabaseStatus::NotFound);
}


TEST_F(SqliteTest, GetData_DatabaseNotFound) {
  std::vector<uint8_t> result;
  ASSERT_EQ(
      transaction_.GetData("bar", kStoreName, existing_data_key_, &result),
      DatabaseStatus::NotFound);
}

TEST_F(SqliteTest, GetData_StoreNotFound) {
  std::vector<uint8_t> result;
  ASSERT_EQ(transaction_.GetData(kDbName, "bar", existing_data_key_, &result),
            DatabaseStatus::NotFound);
}

TEST_F(SqliteTest, GetData_KeyNotFound) {
  std::vector<uint8_t> result;
  ASSERT_EQ(transaction_.GetData(kDbName, kStoreName, 123, &result),
            DatabaseStatus::NotFound);
}

TEST_F(SqliteTest, UpdateData_Found) {
  ASSERT_EQ(transaction_.UpdateData(kDbName, kStoreName, existing_data_key_,
                                    {4, 5, 6}),
            DatabaseStatus::Success);
  std::vector<uint8_t> result;
  ASSERT_EQ(
      transaction_.GetData(kDbName, kStoreName, existing_data_key_, &result),
      DatabaseStatus::Success);
  EXPECT_EQ(result, std::vector<uint8_t>({4, 5, 6}));
}

TEST_F(SqliteTest, UpdateData_DbNotFound) {
  ASSERT_EQ(
      transaction_.UpdateData("foo", kStoreName, existing_data_key_, {4, 5, 6}),
      DatabaseStatus::NotFound);
}

TEST_F(SqliteTest, UpdateData_StoreNotFound) {
  ASSERT_EQ(
      transaction_.UpdateData(kDbName, "foo", existing_data_key_, {4, 5, 6}),
      DatabaseStatus::NotFound);
}

TEST_F(SqliteTest, UpdateData_KeyNotFound) {
  ASSERT_EQ(transaction_.UpdateData(kDbName, kStoreName, 123, {4, 5, 6}),
            DatabaseStatus::Success);
  std::vector<uint8_t> result;
  ASSERT_EQ(transaction_.GetData(kDbName, kStoreName, 123, &result),
            DatabaseStatus::Success);
  EXPECT_EQ(result, std::vector<uint8_t>({4, 5, 6}));
}

TEST_F(SqliteTest, UpdateData_ExplicitKeyChangesIncrementKey) {
  ASSERT_EQ(transaction_.UpdateData(kDbName, kStoreName, 123, {4, 5, 6}),
            DatabaseStatus::Success);
  int64_t new_key;
  ASSERT_EQ(transaction_.AddData(kDbName, kStoreName, {1, 2, 3}, &new_key),
            DatabaseStatus::Success);
  EXPECT_GT(new_key, 123);
}

TEST_F(SqliteTest, DeleteData_Found) {
  ASSERT_EQ(transaction_.DeleteData(kDbName, kStoreName, existing_data_key_),
            DatabaseStatus::Success);
  std::vector<uint8_t> result;
  ASSERT_EQ(
      transaction_.GetData(kDbName, kStoreName, existing_data_key_, &result),
      DatabaseStatus::NotFound);
}

TEST_F(SqliteTest, DeleteData_DbNotFound) {
  ASSERT_EQ(transaction_.DeleteData("foo", kStoreName, existing_data_key_),
            DatabaseStatus::Success);
}

TEST_F(SqliteTest, DeleteData_StoreNotFound) {
  ASSERT_EQ(transaction_.DeleteData(kDbName, "foo", existing_data_key_),
            DatabaseStatus::Success);
}

TEST_F(SqliteTest, DeleteData_KeyNotFound) {
  ASSERT_EQ(transaction_.DeleteData(kDbName, kStoreName, 123),
            DatabaseStatus::Success);
}


TEST_F(SqliteTest, MultipleStores) {
  int64_t new_key;
  ASSERT_EQ(transaction_.AddData(kDbName, kStoreName, {4, 5, 6}, &new_key),
            DatabaseStatus::Success);
  ASSERT_EQ(transaction_.CreateObjectStore(kDbName, "foo"),
            DatabaseStatus::Success);
  int64_t new_key2;
  ASSERT_EQ(transaction_.AddData(kDbName, "foo", {7, 8, 9}, &new_key2),
            DatabaseStatus::Success);

  std::vector<uint8_t> result;
  ASSERT_EQ(transaction_.GetData(kDbName, kStoreName, new_key, &result),
            DatabaseStatus::Success);
  EXPECT_EQ(result, std::vector<uint8_t>({4, 5, 6}));
  ASSERT_EQ(transaction_.GetData(kDbName, "foo", new_key2, &result),
            DatabaseStatus::Success);
  EXPECT_EQ(result, std::vector<uint8_t>({7, 8, 9}));
  ASSERT_EQ(
      transaction_.GetData(kDbName, kStoreName, existing_data_key_, &result),
      DatabaseStatus::Success);
  EXPECT_EQ(result, std::vector<uint8_t>({1, 2, 3}));

  // Even with the same key, different object stores should be different.
  //
  // IMPLEMENTATION DETAIL: Keys are numbered from 1+ and numbered separately
  // in different object stores, so these should have the same value:
  EXPECT_EQ(existing_data_key_, new_key2);
  ASSERT_EQ(
      transaction_.UpdateData(kDbName, kStoreName, existing_data_key_, {10}),
      DatabaseStatus::Success);
  ASSERT_EQ(
      transaction_.GetData(kDbName, kStoreName, existing_data_key_, &result),
      DatabaseStatus::Success);
  EXPECT_EQ(result, std::vector<uint8_t>({10}));
  ASSERT_EQ(transaction_.GetData(kDbName, "foo", existing_data_key_, &result),
            DatabaseStatus::Success);
  EXPECT_EQ(result, std::vector<uint8_t>({7, 8, 9}));

  ASSERT_EQ(transaction_.DeleteData(kDbName, "foo", new_key2),
            DatabaseStatus::Success);
  ASSERT_EQ(
      transaction_.GetData(kDbName, kStoreName, existing_data_key_, &result),
      DatabaseStatus::Success);
  ASSERT_EQ(transaction_.GetData(kDbName, "foo", existing_data_key_, &result),
            DatabaseStatus::NotFound);
}

TEST_F(SqliteTest, HandlesSeparateTransactions) {
  // Commit the existing transaction created in SetUp.
  ASSERT_EQ(transaction_.Commit(), DatabaseStatus::Success);

  int64_t new_key;
  int64_t roll_back_delete;
  int64_t roll_back_create;
  int64_t roll_back_create_new_store;
  {
    SqliteTransaction trans;
    ASSERT_EQ(connection_.BeginTransaction(&trans), DatabaseStatus::Success);

    ASSERT_EQ(trans.AddData(kDbName, kStoreName, {1, 2, 3}, &new_key),
              DatabaseStatus::Success);
    ASSERT_EQ(trans.AddData(kDbName, kStoreName, {1, 2, 3}, &roll_back_delete),
              DatabaseStatus::Success);

    ASSERT_EQ(trans.Commit(), DatabaseStatus::Success);
  }

  {
    SqliteTransaction trans;
    ASSERT_EQ(connection_.BeginTransaction(&trans), DatabaseStatus::Success);

    ASSERT_EQ(trans.CreateObjectStore(kDbName, "foo"), DatabaseStatus::Success);
    ASSERT_EQ(trans.UpdateData(kDbName, kStoreName, new_key, {7, 8, 9}),
              DatabaseStatus::Success);
    ASSERT_EQ(trans.AddData(kDbName, kStoreName, {4, 5, 6}, &roll_back_create),
              DatabaseStatus::Success);
    ASSERT_EQ(
        trans.AddData(kDbName, "foo", {7, 8, 9}, &roll_back_create_new_store),
        DatabaseStatus::Success);
    ASSERT_EQ(trans.DeleteData(kDbName, kStoreName, roll_back_delete),
              DatabaseStatus::Success);

    ASSERT_EQ(trans.Rollback(), DatabaseStatus::Success);
  }

  // Since the above was rolled back, we shouldn't see anything it did.
  {
    SqliteTransaction trans;
    ASSERT_EQ(connection_.BeginTransaction(&trans), DatabaseStatus::Success);

    std::vector<std::string> stores;
    ASSERT_EQ(trans.ListObjectStores(kDbName, &stores),
              DatabaseStatus::Success);
    ASSERT_EQ(stores.size(), 1u);
    ASSERT_EQ(stores[0], kStoreName);

    std::vector<uint8_t> result;
    ASSERT_EQ(trans.GetData(kDbName, kStoreName, new_key, &result),
              DatabaseStatus::Success);
    EXPECT_EQ(result, std::vector<uint8_t>({1, 2, 3}));
    ASSERT_EQ(trans.GetData(kDbName, kStoreName, roll_back_delete, &result),
              DatabaseStatus::Success);
    EXPECT_EQ(result, std::vector<uint8_t>({1, 2, 3}));
    ASSERT_EQ(trans.GetData(kDbName, kStoreName, roll_back_create, &result),
              DatabaseStatus::NotFound);
    ASSERT_EQ(
        trans.GetData(kDbName, "foo", roll_back_create_new_store, &result),
        DatabaseStatus::NotFound);
  }
}


class SqliteFindTest : public SqliteTest {
 public:
  void SetUp() override {
    SqliteTest::SetUp();
    ASSERT_EQ(transaction_.DeleteData(kDbName, kStoreName, existing_data_key_),
              DatabaseStatus::Success);
    ASSERT_EQ(transaction_.UpdateData(kDbName, kStoreName, 5, {1, 2, 3}),
              DatabaseStatus::Success);
    ASSERT_EQ(transaction_.UpdateData(kDbName, kStoreName, 6, {1, 2, 3}),
              DatabaseStatus::Success);
    ASSERT_EQ(transaction_.UpdateData(kDbName, kStoreName, 10, {1, 2, 3}),
              DatabaseStatus::Success);
    ASSERT_EQ(transaction_.UpdateData(kDbName, kStoreName, 11, {1, 2, 3}),
              DatabaseStatus::Success);
  }
};

TEST_F(SqliteFindTest, FindData_DbNotFound) {
  int64_t key;
  ASSERT_EQ(transaction_.FindData("foo", kStoreName, 5, true, &key),
            DatabaseStatus::NotFound);
}

TEST_F(SqliteFindTest, FindData_StoreNotFound) {
  int64_t key;
  ASSERT_EQ(transaction_.FindData(kDbName, "foo", 5, true, &key),
            DatabaseStatus::NotFound);
}

TEST_F(SqliteFindTest, FindData_Empty) {
  ASSERT_EQ(transaction_.CreateObjectStore(kDbName, "foo"),
            DatabaseStatus::Success);
  int64_t key;
  ASSERT_EQ(transaction_.FindData(kDbName, "foo", 5, true, &key),
            DatabaseStatus::NotFound);
}

TEST_F(SqliteFindTest, FindData_Found) {
  int64_t key;
  ASSERT_EQ(transaction_.FindData(kDbName, kStoreName, 5, true, &key),
            DatabaseStatus::Success);
  EXPECT_EQ(key, 6);
  ASSERT_EQ(transaction_.FindData(kDbName, kStoreName, 6, true, &key),
            DatabaseStatus::Success);
  EXPECT_EQ(key, 10);
  ASSERT_EQ(transaction_.FindData(kDbName, kStoreName, 7, true, &key),
            DatabaseStatus::Success);
  EXPECT_EQ(key, 10);
  ASSERT_EQ(transaction_.FindData(kDbName, kStoreName, 10, false, &key),
            DatabaseStatus::Success);
  EXPECT_EQ(key, 6);
  ASSERT_EQ(transaction_.FindData(kDbName, kStoreName, 9, false, &key),
            DatabaseStatus::Success);
  EXPECT_EQ(key, 6);
  ASSERT_EQ(transaction_.FindData(kDbName, kStoreName, 6, false, &key),
            DatabaseStatus::Success);
  EXPECT_EQ(key, 5);
}

TEST_F(SqliteFindTest, FindData_First) {
  int64_t key;
  ASSERT_EQ(transaction_.FindData(kDbName, kStoreName, nullopt, true, &key),
            DatabaseStatus::Success);
  EXPECT_EQ(key, 5);
  ASSERT_EQ(transaction_.FindData(kDbName, kStoreName, nullopt, false, &key),
            DatabaseStatus::Success);
  EXPECT_EQ(key, 11);
}

TEST_F(SqliteFindTest, FindData_End) {
  int64_t key;
  ASSERT_EQ(transaction_.FindData(kDbName, kStoreName, 11, true, &key),
            DatabaseStatus::NotFound);
  ASSERT_EQ(transaction_.FindData(kDbName, kStoreName, 20, true, &key),
            DatabaseStatus::NotFound);
  ASSERT_EQ(transaction_.FindData(kDbName, kStoreName, 5, false, &key),
            DatabaseStatus::NotFound);
  ASSERT_EQ(transaction_.FindData(kDbName, kStoreName, 3, false, &key),
            DatabaseStatus::NotFound);
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
