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

#include "src/memory/heap_tracer.h"

#include <gtest/gtest.h>

#include "src/core/member.h"
#include "src/mapping/backing_object.h"
#include "src/memory/object_tracker.h"

namespace shaka {
namespace memory {

namespace {

class TestObject : public BackingObject {
 public:
  static std::string name() {
    return "";
  }

  void Trace(memory::HeapTracer* tracer) const override {
    // Don't trace JsThis since we don't actually use the value.  We haven't
    // setup the JavaScript engine, so we can't trace any JavaScript objects.
  }

 private:
  BackingObjectFactoryBase* factory() const override {
    return nullptr;
  }
};

class TestObjectWithBackingChild : public TestObject {
 public:
  void Trace(HeapTracer* tracer) const override {
    tracer->Trace(&member1);
    tracer->Trace(&member2);
    tracer->Trace(&member3);
  }

  Member<TestObject> member1;
  Member<TestObject> member2;
  Member<TestObject> member3;
};

}  // namespace

class HeapTracerTest : public testing::Test {
 public:
  HeapTracerTest() : tracker(&heap_tracer) {}
  ~HeapTracerTest() override {
    tracker.Dispose();
  }

 protected:
  template <typename T, typename... Args>
  void ExpectAlive(T arg, Args... args) {
    EXPECT_EQ(1u, heap_tracer.alive().count(arg));
    ExpectAlive(args...);
  }
  void ExpectAlive() {}

  template <typename T, typename... Args>
  void ExpectDead(T arg, Args... args) {
    EXPECT_EQ(0u, heap_tracer.alive().count(arg));
    ExpectDead(args...);
  }
  void ExpectDead() {}

  void RunTracer(const std::unordered_set<const Traceable*>& ref_alive,
                 Traceable* root) {
    heap_tracer.BeginPass();
    heap_tracer.Trace(root);
    heap_tracer.TraceAll(ref_alive);
  }

  ObjectTracker::UnsetForTesting unset_;
  HeapTracer heap_tracer;
  ObjectTracker tracker;
};

TEST_F(HeapTracerTest, BasicFlow) {
  auto* obj1 = new TestObject;
  auto* obj2 = new TestObject;
  auto* obj3 = new TestObject;
  auto* obj4 = new TestObject;

  // Ref-counted alive objects: obj1, obj4.
  std::unordered_set<const Traceable*> ref_alive = {obj1, obj4};

  // JavaScript alive objects: obj3.
  RunTracer(ref_alive, obj3);

  ExpectAlive(obj1, obj3, obj4);
  ExpectDead(obj2);
}

TEST_F(HeapTracerTest, TracesIndirectChildren) {
  // Root (alive) object.
  auto* root = new TestObjectWithBackingChild;
  // Indirect alive objects.
  auto* A = new TestObjectWithBackingChild;
  auto* B = new TestObjectWithBackingChild;
  auto* C = new TestObjectWithBackingChild;
  auto* D = new TestObjectWithBackingChild;
  auto* E = new TestObjectWithBackingChild;
  auto* F = new TestObjectWithBackingChild;
  auto* G = new TestObjectWithBackingChild;

  root->member1 = A;
  root->member2 = B;
  root->member3 = C;
  A->member1 = D;
  A->member2 = E;
  B->member1 = E;
  C->member1 = E;
  C->member2 = F;
  E->member1 = F;
  E->member2 = G;
  // Dead objects
  auto* H = new TestObjectWithBackingChild;
  auto* I = new TestObjectWithBackingChild;
  auto* J = new TestObjectWithBackingChild;
  auto* K = new TestObjectWithBackingChild;
  H->member1 = C;
  H->member2 = J;
  I->member1 = A;
  I->member2 = D;

  // First test using JavaScript alive objects.
  std::unordered_set<const Traceable*> ref_alive;
  RunTracer(ref_alive, root);

  // Check first test results.
  ExpectAlive(root, A, B, C, D, E, F, G);
  ExpectDead(H, I, J, K);

  // Try again using ref-counted alive
  ref_alive = {root};
  RunTracer(ref_alive, nullptr);

  // Check results of second test.
  ExpectAlive(root, A, B, C, D, E, F, G);
  ExpectDead(H, I, J, K);
}

TEST_F(HeapTracerTest, SupportsCircularReferences) {
  // Root (alive) object.
  auto* root = new TestObjectWithBackingChild;
  // Indirect alive objects.
  auto* A = new TestObjectWithBackingChild;
  auto* B = new TestObjectWithBackingChild;
  auto* C = new TestObjectWithBackingChild;
  root->member1 = A;
  A->member1 = B;
  A->member2 = C;
  B->member1 = root;

  // First test using JavaScript alive objects.
  std::unordered_set<const Traceable*> ref_alive;
  RunTracer(ref_alive, root);

  // Check first test results.
  ExpectAlive(root, A, B, C);

  // Try again using ref-counted alive
  ref_alive = {root};
  RunTracer(ref_alive, nullptr);

  // Check results of second test.
  ExpectAlive(root, A, B, C);
}

}  // namespace memory
}  // namespace shaka
