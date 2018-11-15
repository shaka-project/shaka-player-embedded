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

#include "src/core/ref_ptr.h"

#include <gtest/gtest.h>

#include <type_traits>
#include <unordered_map>

#include "src/core/member.h"
#include "src/mapping/convert_js.h"
#include "src/mapping/js_wrappers.h"
#include "src/memory/heap_tracer.h"
#include "src/memory/object_tracker.h"

namespace shaka {

namespace {
struct Base : BackingObject {
  static std::string name() {
    return "Base";
  }

  ReturnVal<JsValue> JsThis() {
    CHECK(false);
  }
  void Trace(memory::HeapTracer*) const override {}
  BackingObjectFactoryBase* factory() const override {
    return nullptr;
  }

  int i = 12;
  std::string j = "abc";
};
struct Derived : Base {};

}  // namespace


class RefPtrTest : public testing::Test {
 public:
  ~RefPtrTest() override {
    tracker_.UnregisterAllObjects();
  }

 protected:
  uint32_t GetRefCount(memory::Traceable* object) const {
    return tracker_.GetRefCount(object);
  }

  void ExpectEmptyTracker() {
    EXPECT_EQ(0u, tracker_.GetRefCount(&base1_));
    EXPECT_EQ(0u, tracker_.GetRefCount(&base2_));
    EXPECT_EQ(0u, tracker_.GetRefCount(&derived_));
  }

  memory::ObjectTracker::UnsetForTesting unset_;
  memory::ObjectTracker tracker_;
  Base base1_;
  Base base2_;
  Derived derived_;
};

TEST_F(RefPtrTest, BasicFlow) {
  {
    RefPtr<Base> ptr1(&base1_);
    EXPECT_EQ(1u, GetRefCount(&base1_));
    EXPECT_EQ(0u, GetRefCount(&base2_));

    ptr1 = &base2_;
    EXPECT_EQ(0u, GetRefCount(&base1_));
    EXPECT_EQ(1u, GetRefCount(&base2_));

    RefPtr<Base> ptr2(&base2_);
    EXPECT_EQ(0u, GetRefCount(&base1_));
    EXPECT_EQ(2u, GetRefCount(&base2_));
  }

  ExpectEmptyTracker();
}

TEST_F(RefPtrTest, SupportsCopyAndMove) {
  {
    RefPtr<Base> ptr1(&base1_);
    RefPtr<Base> ptr2(ptr1);
    RefPtr<Base> ptr3;
    EXPECT_TRUE(ptr3.empty());
    ptr3 = ptr1;
    EXPECT_EQ(3u, GetRefCount(&base1_));
    EXPECT_EQ(0u, GetRefCount(&base2_));
    EXPECT_EQ(ptr1, ptr2);
    EXPECT_EQ(ptr1, ptr3);

    EXPECT_FALSE(ptr2.empty());
    RefPtr<Base> ptr4(std::move(ptr2));
    EXPECT_TRUE(ptr2.empty());
    EXPECT_FALSE(ptr4.empty());
    EXPECT_EQ(3u, GetRefCount(&base1_));
    EXPECT_EQ(ptr1, ptr4);
    EXPECT_NE(ptr2, ptr4);

    ptr2 = std::move(ptr4);
    EXPECT_TRUE(ptr4.empty());
    EXPECT_FALSE(ptr2.empty());
    EXPECT_EQ(3u, GetRefCount(&base1_));

    ptr1 = ptr2;
    EXPECT_EQ(3u, GetRefCount(&base1_));

    ptr1 = nullptr;
    EXPECT_EQ(2u, GetRefCount(&base1_));
    EXPECT_TRUE(ptr1.empty());

    ptr2.reset();
    EXPECT_EQ(1u, GetRefCount(&base1_));
    EXPECT_TRUE(ptr2.empty());
    EXPECT_FALSE(ptr3.empty());
  }

  ExpectEmptyTracker();
}

TEST_F(RefPtrTest, SupportsCallingMethods) {
  auto VerifyMethodArguments = [this](RefPtr<Base> copy, RefPtr<Base> move) {
    // Cannot use an rvalue reference for |move| because the ref count would
    // not be correct.  When we don't use a reference, the value will be
    // move-constructed into the argument, which is why the ref count is 1.
    // When the method returns, the argument is destroyed, dropping the ref
    // count to 0.
    //
    // If we used an rvalue reference, we would pass a reference and not
    // create a temporary.  What makes the ref count change is the
    // constructor so because no constructor is invoked, the original
    // variable would remain unchanged.
    EXPECT_EQ(2u, GetRefCount(copy.get()));
    EXPECT_EQ(1u, GetRefCount(move.get()));
  };

  {
    RefPtr<Base> ptr1(&base1_);
    RefPtr<Base> ptr2(&base2_);
    EXPECT_EQ(1u, GetRefCount(&base1_));
    EXPECT_EQ(1u, GetRefCount(&base2_));

    VerifyMethodArguments(ptr1, std::move(ptr2));

    EXPECT_FALSE(ptr1.empty());
    EXPECT_TRUE(ptr2.empty());
    EXPECT_EQ(1u, GetRefCount(&base1_));
    EXPECT_EQ(0u, GetRefCount(&base2_));
  }

  ExpectEmptyTracker();
}

TEST_F(RefPtrTest, SupportsComparisons) {
  RefPtr<Base> ptr1(&base1_);

  EXPECT_FALSE(ptr1.empty());
  EXPECT_TRUE(ptr1 == &base1_);
  EXPECT_TRUE(&base1_ == ptr1);
  EXPECT_FALSE(ptr1 != &base1_);
  EXPECT_FALSE(&base1_ != ptr1);
  EXPECT_FALSE(ptr1 == &base2_);
  EXPECT_TRUE(ptr1 != &base2_);
  EXPECT_FALSE(ptr1 == &derived_);
  EXPECT_TRUE(ptr1 != &derived_);
  EXPECT_FALSE(ptr1 == nullptr);
  EXPECT_TRUE(ptr1 != nullptr);
  EXPECT_FALSE(nullptr == ptr1);
  EXPECT_TRUE(nullptr != ptr1);

  ptr1 = &derived_;

  EXPECT_FALSE(ptr1.empty());
  EXPECT_FALSE(ptr1 == &base1_);
  EXPECT_TRUE(ptr1 != &base1_);
  EXPECT_TRUE(ptr1 == &derived_);
  EXPECT_FALSE(ptr1 != &derived_);
  EXPECT_FALSE(ptr1 == nullptr);
  EXPECT_TRUE(ptr1 != nullptr);

  ptr1 = nullptr;

  EXPECT_TRUE(ptr1.empty());
  EXPECT_FALSE(ptr1 == &base1_);
  EXPECT_TRUE(ptr1 != &base1_);
  EXPECT_FALSE(ptr1 == &derived_);
  EXPECT_TRUE(ptr1 != &derived_);
  EXPECT_TRUE(ptr1 == nullptr);
  EXPECT_FALSE(ptr1 != nullptr);

  Member<Base> mem1(&base1_);
  Member<Derived> mem2(&derived_);
  ptr1 = &base1_;
  EXPECT_TRUE(ptr1 == mem1);
  EXPECT_TRUE(mem1 == ptr1);
  EXPECT_FALSE(ptr1 == mem2);
  EXPECT_FALSE(mem2 == ptr1);
  EXPECT_FALSE(ptr1 != mem1);
  EXPECT_FALSE(mem1 != ptr1);
  EXPECT_TRUE(ptr1 != mem2);
  EXPECT_TRUE(mem2 != ptr1);
}

TEST_F(RefPtrTest, InteractsWithMember) {
  {
    RefPtr<Base> ptr1(&base1_);
    Member<Base> mem1(ptr1);
    EXPECT_EQ(mem1, ptr1);
    EXPECT_EQ(ptr1, mem1);

    Member<Base> mem2(std::move(ptr1));
    EXPECT_TRUE(ptr1.empty());
    EXPECT_TRUE(mem2 == &base1_);

    ptr1 = mem1;
    EXPECT_FALSE(ptr1.empty());
    EXPECT_EQ(ptr1, &base1_);

    ptr1.reset();
    ptr1 = std::move(mem1);
    EXPECT_TRUE(mem1.empty());
    EXPECT_FALSE(ptr1.empty());
    EXPECT_EQ(ptr1, &base1_);

    RefPtr<Base> ptr2(std::move(mem2));
    EXPECT_TRUE(mem2.empty());
    EXPECT_FALSE(ptr2.empty());
    EXPECT_EQ(ptr2, &base1_);

    mem2 = ptr1;
    EXPECT_FALSE(mem2.empty());
    EXPECT_FALSE(ptr1.empty());
    EXPECT_EQ(mem2, &base1_);

    Member<Derived> mem3(&derived_);
    ptr1 = mem3;
    EXPECT_FALSE(ptr1.empty());
    EXPECT_EQ(ptr1, &derived_);

    ptr1.reset();
    EXPECT_TRUE(ptr1.empty());
    ptr1 = std::move(mem3);
    EXPECT_TRUE(mem3.empty());
    EXPECT_FALSE(ptr1.empty());
    EXPECT_EQ(ptr1, &derived_);

    mem3 = &derived_;
    RefPtr<Base> ptr3(mem3);
    EXPECT_FALSE(ptr3.empty());
    EXPECT_EQ(ptr3, mem3);
  }

  ExpectEmptyTracker();
}

}  // namespace shaka
