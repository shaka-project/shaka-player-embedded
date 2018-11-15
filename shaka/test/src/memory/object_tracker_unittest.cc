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

#include "src/memory/object_tracker.h"

#include <gtest/gtest.h>

#include <functional>

#include "src/core/ref_ptr.h"
#include "src/mapping/backing_object.h"
#include "src/util/utils.h"

namespace shaka {
namespace memory {

namespace {

class TestObject : public BackingObject {
 public:
  static std::string name() {
    return "TestObject";
  }

  TestObject(bool* is_free) : is_free_(is_free) {
    *is_free = false;
  }
  ~TestObject() override {
    *is_free_ = true;
    if (on_destroy)
      on_destroy();
  }

  std::function<void()> on_destroy;

 private:
  BackingObjectFactoryBase* factory() const override {
    return nullptr;
  }

  bool* is_free_;
};

}  // namespace

class ObjectTrackerTest : public testing::Test {
 protected:
  void ExpectNonZeroRefs(const Traceable* obj) {
    EXPECT_TRUE(util::contains(tracker.GetAliveObjects(), obj));
  }
  void ExpectZeroRefs(const Traceable* obj) {
    EXPECT_FALSE(util::contains(tracker.GetAliveObjects(), obj));
    EXPECT_TRUE(util::contains(tracker.GetAllObjects(), obj));
  }
  void ExpectMissing(const Traceable* obj) {
    EXPECT_FALSE(util::contains(tracker.GetAllObjects(), obj));
  }

  ObjectTracker::UnsetForTesting unset_;
  ObjectTracker tracker;
};

TEST_F(ObjectTrackerTest, BasicFlow) {
  bool is_free = false;
  TestObject* obj = new TestObject(&is_free);
  ASSERT_TRUE(obj);
  ExpectZeroRefs(obj);
  ASSERT_FALSE(is_free);

  {
    // Note, this will not free the object even though it is the last reference.
    RefPtr<TestObject> ref(obj);
    ExpectNonZeroRefs(obj);
  }
  ASSERT_FALSE(is_free);
  ExpectZeroRefs(obj);

  // The tracker thinks the object is dead because it has a zero ref count.  But
  // it will not be freed because it is in |js_alive|.
  std::unordered_set<const Traceable*> js_alive = {obj};
  tracker.FreeDeadObjects(js_alive);
  ExpectZeroRefs(obj);
  ASSERT_FALSE(is_free);

  // Perform a GC where the object is dead.
  ExpectZeroRefs(obj);
  js_alive.clear();
  tracker.FreeDeadObjects(js_alive);
  // The pointer is invalid at this point.
  ExpectMissing(obj);
  EXPECT_TRUE(is_free);
}

TEST_F(ObjectTrackerTest, Dispose) {
  bool is_free1, is_free2;
  TestObject* obj1 = new TestObject(&is_free1);
  TestObject* obj2 = new TestObject(&is_free2);
  tracker.AddRef(obj1);
  ExpectNonZeroRefs(obj1);
  ExpectZeroRefs(obj2);

  tracker.Dispose();

  ExpectMissing(obj1);
  ExpectMissing(obj2);
  EXPECT_TRUE(is_free1);
  EXPECT_TRUE(is_free2);
}

TEST_F(ObjectTrackerTest, CanCreateObjectsInDispose) {
  bool is_free1, is_free2 = false, is_free3;
  TestObject* obj1 = new TestObject(&is_free1);

  obj1->on_destroy = [&]() {
    TestObject* obj2 = new TestObject(&is_free2);
    obj2->on_destroy = [&]() { new TestObject(&is_free3); };
  };

  tracker.Dispose();

  EXPECT_TRUE(is_free1);
  EXPECT_TRUE(is_free2);
  EXPECT_TRUE(is_free3);
}

TEST_F(ObjectTrackerTest, RefCounts) {
  bool is_free1, is_free2;
  TestObject* obj1 = new TestObject(&is_free1);
  TestObject* obj2 = new TestObject(&is_free2);

  // Basic flow.
  ExpectZeroRefs(obj1);
  tracker.AddRef(obj1);
  ExpectNonZeroRefs(obj1);
  tracker.RemoveRef(obj1);
  ExpectZeroRefs(obj1);

  // Two objects are independent.
  tracker.AddRef(obj1);
  tracker.AddRef(obj2);
  ExpectNonZeroRefs(obj1);
  ExpectNonZeroRefs(obj2);
  tracker.RemoveRef(obj2);
  ExpectNonZeroRefs(obj1);
  ExpectZeroRefs(obj2);
  tracker.RemoveRef(obj1);
  ExpectZeroRefs(obj1);
  ExpectZeroRefs(obj2);

  // Multiple ref counts.
  tracker.AddRef(obj1);
  tracker.AddRef(obj2);
  tracker.AddRef(obj2);
  tracker.AddRef(obj1);
  tracker.AddRef(obj1);
  tracker.AddRef(obj2);
  ExpectNonZeroRefs(obj1);
  ExpectNonZeroRefs(obj2);
  tracker.RemoveRef(obj1);
  tracker.RemoveRef(obj1);
  ExpectNonZeroRefs(obj1);
  ExpectNonZeroRefs(obj2);
  tracker.RemoveRef(obj2);  // obj1 = 1, obj2 = 2
  ExpectNonZeroRefs(obj1);
  ExpectNonZeroRefs(obj2);
  tracker.AddRef(obj1);
  tracker.RemoveRef(obj2);
  tracker.RemoveRef(obj2);  // obj1 = 2, obj2 = 0
  ExpectNonZeroRefs(obj1);
  ExpectZeroRefs(obj2);
  tracker.RemoveRef(obj1);
  tracker.RemoveRef(obj1);
  ExpectZeroRefs(obj1);
  ExpectZeroRefs(obj2);

  EXPECT_FALSE(is_free1);
  EXPECT_FALSE(is_free2);
  tracker.Dispose();
}

}  // namespace memory
}  // namespace shaka
