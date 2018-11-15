// Copyright 2018 Google LLC
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

#include "shaka/variant.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace shaka {

namespace {

class Singleton {
 public:
  Singleton() {
    EXPECT_FALSE(constructed);
    constructed = true;
  }
  Singleton(Singleton&&) = delete;
  Singleton(const Singleton&) = delete;
  ~Singleton() {
    EXPECT_TRUE(constructed);
    constructed = false;
  }

  Singleton& operator=(Singleton&&) = delete;
  Singleton& operator=(const Singleton&) = delete;

  static bool constructed;
};
bool Singleton::constructed = false;


struct MockAllocator {
  MOCK_METHOD0(DefaultCtor, void());
  MOCK_METHOD0(CopyCtor, void());
  MOCK_METHOD0(MoveCtor, void());
  MOCK_METHOD0(Dtor, void());
};

class AllocTracker {
 public:
  AllocTracker() {
    alloc->DefaultCtor();
  }
  AllocTracker(const AllocTracker&) {
    alloc->CopyCtor();
  }
  AllocTracker(AllocTracker&&) {
    alloc->MoveCtor();
  }
  ~AllocTracker() {
    alloc->Dtor();
  }

  AllocTracker& operator=(const AllocTracker&) = delete;
  AllocTracker& operator=(AllocTracker&&) = delete;

  static MockAllocator* alloc;
};
MockAllocator* AllocTracker::alloc = nullptr;


struct CustomType {
  explicit CustomType(const std::string& str, int i) : str(str), i(i) {}

  const std::string str;
  const int i;
};

}  // namespace

using testing::InSequence;
using testing::MockFunction;
using testing::StrictMock;

TEST(Variant, ConstructsFirstType) {
  EXPECT_FALSE(Singleton::constructed);
  {
    variant<Singleton, std::string> foo;
    EXPECT_EQ(foo.index(), 0u);
    EXPECT_TRUE(Singleton::constructed);
  }
  EXPECT_FALSE(Singleton::constructed);
}

TEST(Variant, SupportsComplexAssignments) {
  StrictMock<MockAllocator> alloc;
  MockFunction<void(int)> call;
  AllocTracker::alloc = &alloc;

  {
    InSequence seq;
    EXPECT_CALL(alloc, DefaultCtor());  // start
    EXPECT_CALL(alloc, CopyCtor());     // foo
    EXPECT_CALL(alloc, MoveCtor());     // bar
    EXPECT_CALL(call, Call(1));
    EXPECT_CALL(alloc, DefaultCtor());  // baz
    EXPECT_CALL(alloc, Dtor());         // baz dealloc
    EXPECT_CALL(call, Call(2));
    EXPECT_CALL(alloc, Dtor());  // bar
    EXPECT_CALL(alloc, Dtor());  // foo
    EXPECT_CALL(call, Call(3));

    EXPECT_CALL(alloc, CopyCtor());     // foo
    EXPECT_CALL(alloc, Dtor());         // foo
    EXPECT_CALL(alloc, DefaultCtor());  // foo emplace<0>()
    EXPECT_CALL(call, Call(4));
    EXPECT_CALL(alloc, Dtor());         // foo.emplace<AllocTracker>();
    EXPECT_CALL(alloc, DefaultCtor());  // cont.
    EXPECT_CALL(call, Call(5));
    EXPECT_CALL(alloc, Dtor());  // foo.emplace<bool>(false);
    EXPECT_CALL(call, Call(6));

    EXPECT_CALL(alloc, DefaultCtor());  // foo
    EXPECT_CALL(alloc, DefaultCtor());  // bar
    EXPECT_CALL(alloc, Dtor());         // foo assign dealloc
    EXPECT_CALL(alloc, CopyCtor());     // foo assign copy
    EXPECT_CALL(alloc, Dtor());         // foo assign dealloc
    EXPECT_CALL(alloc, MoveCtor());     // foo assign copy
    EXPECT_CALL(alloc, Dtor());         // bar
    EXPECT_CALL(alloc, Dtor());         // foo
    EXPECT_CALL(call, Call(7));

    EXPECT_CALL(alloc, DefaultCtor());  // foo
    EXPECT_CALL(alloc, Dtor());         // foo assign dealloc
    EXPECT_CALL(call, Call(8));

    EXPECT_CALL(alloc, Dtor());  // start
  }

  {
    variant<AllocTracker> start;
    {
      variant<AllocTracker> foo = start;
      variant<AllocTracker> bar = std::move(foo);
      call.Call(1);

      variant<AllocTracker, bool> baz;
      baz = true;
      call.Call(2);
    }
    call.Call(3);
    {
      variant<AllocTracker, bool> foo = get<0>(start);
      foo = false;
      foo.emplace<0>();
      call.Call(4);
      foo.emplace<AllocTracker>();
      call.Call(5);
      foo.emplace<bool>(false);
    }
    call.Call(6);
    {
      variant<AllocTracker> foo;
      variant<AllocTracker> bar;
      foo = bar;
      foo = std::move(bar);
    }
    call.Call(7);
    {
      variant<AllocTracker, bool> foo;
      variant<AllocTracker, bool> bar(false);
      foo = bar;
      call.Call(8);
      variant<AllocTracker, bool> baz(false);
      foo = std::move(baz);
    }
  }
}

TEST(Variant, ConstructsBasedOnArgument) {
  EXPECT_FALSE(Singleton::constructed);
  {
    variant<Singleton, std::string> foo("foo");
    EXPECT_FALSE(Singleton::constructed);
    EXPECT_EQ(foo.index(), 1u);
  }
  EXPECT_FALSE(Singleton::constructed);
}

TEST(Variant, SupportsIndexEmplace) {
  EXPECT_FALSE(Singleton::constructed);
  {
    variant<Singleton, std::string> foo;
    EXPECT_TRUE(Singleton::constructed);
    EXPECT_EQ(foo.index(), 0u);
    foo.emplace<1>("foo");
    EXPECT_FALSE(Singleton::constructed);
    EXPECT_EQ(foo.index(), 1u);
  }
  EXPECT_FALSE(Singleton::constructed);
}

TEST(Variant, SupportsTypeEmplace) {
  EXPECT_FALSE(Singleton::constructed);
  {
    variant<Singleton, std::string> foo;
    EXPECT_TRUE(Singleton::constructed);
    EXPECT_EQ(foo.index(), 0u);
    foo.emplace<std::string>("foo");
    EXPECT_FALSE(Singleton::constructed);
    EXPECT_EQ(foo.index(), 1u);
  }
  EXPECT_FALSE(Singleton::constructed);
}

TEST(Variant, SupportsAssignment) {
  variant<double, std::string> foo;
  EXPECT_EQ(foo.index(), 0u);
  foo = "foo";
  EXPECT_EQ(foo.index(), 1u);
}

TEST(Variant, SupportsComparison) {
  variant<double, std::string> a(10);
  variant<double, std::string> b(10);
  variant<double, std::string> c = b;
  variant<double, std::string> d(20);
  variant<double, std::string> e("foo");

  EXPECT_TRUE(a == b);
  EXPECT_TRUE(b == a);
  EXPECT_TRUE(a == c);
  EXPECT_FALSE(a == d);
  EXPECT_FALSE(a == e);

  EXPECT_FALSE(a != b);
  EXPECT_FALSE(b != a);
  EXPECT_FALSE(a != c);
  EXPECT_TRUE(a != d);
  EXPECT_TRUE(a != e);
}

TEST(Variant, SupportsGettingValue) {
  variant<double, std::string> a(10);

  EXPECT_TRUE(holds_alternative<double>(a));
  EXPECT_FALSE(holds_alternative<std::string>(a));
  EXPECT_EQ(get<double>(a), 10);
  EXPECT_EQ(get<0>(a), 10);
  ASSERT_NE(get_if<0>(a), nullptr);
  EXPECT_EQ(*get_if<0>(a), 10);
  EXPECT_EQ(get_if<1>(a), nullptr);
  ASSERT_NE(get_if<double>(a), nullptr);
  EXPECT_EQ(*get_if<double>(a), 10);
  EXPECT_EQ(get_if<std::string>(a), nullptr);

  a = 20;
  EXPECT_TRUE(holds_alternative<double>(a));
  EXPECT_FALSE(holds_alternative<std::string>(a));
  EXPECT_EQ(get<double>(a), 20);
  EXPECT_EQ(get<0>(a), 20);
  ASSERT_NE(get_if<0>(a), nullptr);
  EXPECT_EQ(*get_if<0>(a), 20);
  EXPECT_EQ(get_if<1>(a), nullptr);
  ASSERT_NE(get_if<double>(a), nullptr);
  EXPECT_EQ(*get_if<double>(a), 20);
  EXPECT_EQ(get_if<std::string>(a), nullptr);

  a = "foobar";
  EXPECT_FALSE(holds_alternative<double>(a));
  EXPECT_TRUE(holds_alternative<std::string>(a));
  EXPECT_EQ(get<std::string>(a), "foobar");
  EXPECT_EQ(get<1>(a), "foobar");
  EXPECT_EQ(get_if<0>(a), nullptr);
  ASSERT_NE(get_if<1>(a), nullptr);
  EXPECT_EQ(*get_if<1>(a), "foobar");
  EXPECT_EQ(get_if<double>(a), nullptr);
  ASSERT_NE(get_if<std::string>(a), nullptr);
  EXPECT_EQ(*get_if<std::string>(a), "foobar");

  const variant<double, std::string> const_ = "foobar";
  EXPECT_TRUE(holds_alternative<std::string>(const_));
  EXPECT_EQ(get<std::string>(const_), "foobar");
  EXPECT_NE(get_if<std::string>(const_), nullptr);
  EXPECT_EQ(*get_if<std::string>(const_), "foobar");
}

TEST(Variant, SupportsTwoArgumentConstructors) {
  variant<CustomType> foo(CustomType("foo", 10));
  EXPECT_EQ(get<0>(foo).str, "foo");
  EXPECT_EQ(get<0>(foo).i, 10);

  foo.emplace<CustomType>("bar", 20);
  EXPECT_EQ(get<0>(foo).str, "bar");
  EXPECT_EQ(get<0>(foo).i, 20);
}

TEST(Variant, SupportsDuplicateTypes) {
  // Note that you can't use the typed versions of these because they only work
  // if the type is unique.
  variant<std::string, std::string> foo;
  EXPECT_EQ(foo.index(), 0u);
  EXPECT_EQ(get<0>(foo), "");
  EXPECT_NE(get_if<0>(foo), nullptr);
  EXPECT_EQ(get_if<1>(foo), nullptr);

  foo.emplace<1>("bar");
  EXPECT_EQ(foo.index(), 1u);
  EXPECT_EQ(get<1>(foo), "bar");
  EXPECT_EQ(get_if<0>(foo), nullptr);
  EXPECT_NE(get_if<1>(foo), nullptr);
}

}  // namespace shaka
