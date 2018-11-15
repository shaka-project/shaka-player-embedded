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

// TODO: Add tests for JSC.
#ifdef USING_V8

#  include <gtest/gtest.h>

#  include <array>
#  include <functional>

#  include "src/core/js_manager_impl.h"
#  include "src/core/member.h"
#  include "src/core/ref_ptr.h"
#  include "src/mapping/backing_object.h"
#  include "src/mapping/backing_object_factory.h"
#  include "src/mapping/js_wrappers.h"
#  include "src/mapping/weak_js_ptr.h"
#  include "src/memory/heap_tracer.h"
#  include "src/memory/v8_heap_tracer.h"
#  include "src/test/v8_test.h"
#  include "src/util/pseudo_singleton.h"

#  define DEFINE_HANDLES(var) v8::HandleScope var(isolate())

namespace shaka {
namespace memory {

namespace {

void Noop() {}

class TestObject : public BackingObject {
  DECLARE_TYPE_INFO(TestObject);

 public:
  TestObject(bool* is_free) : is_free_(is_free) {
    *is_free = false;
  }

  bool has_been_traced() const {
    return has_been_traced_;
  }

  void Trace(HeapTracer* tracer) const override {
    BackingObject::Trace(tracer);

    has_been_traced_ = true;
    tracer->Trace(&member1);
    tracer->Trace(&member2);
    tracer->Trace(&v8_member);

    on_trace();
  }

  Member<TestObject> member1;
  Member<TestObject> member2;

  WeakJsPtr<JsObject> v8_member;

  std::function<void()> on_destroy = &Noop;
  std::function<void()> on_trace = &Noop;

 private:
  bool* is_free_;
  mutable bool has_been_traced_ = false;
};

TestObject::~TestObject() {
  EXPECT_FALSE(*is_free_);
  *is_free_ = true;
  on_destroy();
}

class ReusableTestObject : public TestObject {
 public:
  ReusableTestObject(int* free_count)
      : TestObject(&is_free_), free_count_(free_count) {}

  ~ReusableTestObject() override {
    ++*free_count_;
  }

  static void operator delete(void* ptr) {
    // Don't free it since the object is stack allocated.
  }

 private:
  int* free_count_;
  bool is_free_;
};

class TestObjectFactory : public BackingObjectFactory<TestObject> {
 public:
  TestObjectFactory() {}
};

}  // namespace

class ObjectTrackerIntegration
    : public V8Test,
      public PseudoSingleton<ObjectTrackerIntegration> {
 public:
  static BackingObjectFactoryBase* factory() {
    return Instance()->factory_.get();
  }

  void SetUp() override {
    V8Test::SetUp();
    isolate()->SetEmbedderHeapTracer(&v8_heap_tracer_);
    factory_.reset(new TestObjectFactory);
  }

  void TearDown() override {
    // Perform a full V8 GC to clean up any leftover objects.
    tracker_.Dispose();
    factory_.reset();

    V8Test::TearDown();
  }

 protected:
  ReturnVal<JsValue> Wrap(TestObject* ptr) {
    return factory_->WrapInstance(ptr);
  }

  void SetMember(Handle<JsObject> obj, const std::string& name,
                 TestObject* ptr) {
    DEFINE_HANDLES(handles);
    LocalVar<JsValue> value;
    if (!ptr)
      value = JsUndefined();
    else
      value = Wrap(ptr);
    SetMemberRaw(obj, name, value);
  }

  void SetMember(WeakJsPtr<JsObject> obj, const std::string& name,
                 TestObject* ptr) {
    DEFINE_HANDLES(handles);
    SetMember(obj.handle(), name, ptr);
  }

  void SetMember(WeakJsPtr<JsObject> obj, const std::string& name,
                 WeakJsPtr<JsObject> other) {
    DEFINE_HANDLES(handles);
    LocalVar<JsValue> value(other.value());
    SetMemberRaw(obj.handle(), name, value);
  }

  void SetGlobal(const std::string& name, TestObject* ptr) {
    DEFINE_HANDLES(handles);
    SetMember(JsEngine::Instance()->global_handle(), name, ptr);
  }

  WeakJsPtr<JsObject> CreateWeakObject() {
    DEFINE_HANDLES(handles);
    return CreateObject();
  }

  void RunGc() {
    isolate()->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection);
  }

  ObjectTracker tracker_;
  V8HeapTracer v8_heap_tracer_{tracker_.heap_tracer(), &tracker_};
  std::unique_ptr<TestObjectFactory> factory_;
};

BackingObjectFactoryBase* TestObject::factory() const {
  return ObjectTrackerIntegration::Instance()->factory();
}

TEST_F(ObjectTrackerIntegration, BasicFlow) {
  bool is_free1, is_free2, is_free3;
  RefPtr<TestObject> obj1(new TestObject(&is_free1));
  {
    RefPtr<TestObject> obj2(new TestObject(&is_free2));
    new TestObject(&is_free3);
  }
  EXPECT_FALSE(is_free1);
  EXPECT_FALSE(is_free2);
  EXPECT_FALSE(is_free3);

  // obj1 is still alive, so should not get collected.
  RunGc();
  EXPECT_FALSE(is_free1);
  EXPECT_TRUE(is_free2);
  EXPECT_TRUE(is_free3);

  obj1.reset();
  RunGc();
  EXPECT_TRUE(is_free1);
}

TEST_F(ObjectTrackerIntegration, AliveThroughJavaScript) {
  bool is_free1, is_free2;
  TestObject* obj1(new TestObject(&is_free1));
  new TestObject(&is_free2);

  SetGlobal("key", obj1);

  // We don't hold a C++ reference to it, but JavaScript does so it should not
  // be freed.
  RunGc();
  EXPECT_FALSE(is_free1);
  EXPECT_TRUE(is_free2);

  // Un-setting the JavaScript variable, then the object should be freed.
  SetGlobal("key", nullptr);
  RunGc();
  EXPECT_TRUE(is_free1);
  EXPECT_TRUE(is_free2);
}

TEST_F(ObjectTrackerIntegration, AliveIndirectly) {
  bool is_free_root, is_free1, is_free2, is_free3, is_free_dead;
  TestObject* root = new TestObject(&is_free_root);
  TestObject* obj1 = new TestObject(&is_free1);
  TestObject* obj2 = new TestObject(&is_free2);
  TestObject* obj3 = new TestObject(&is_free3);
  TestObject* dead = new TestObject(&is_free_dead);

  root->member1 = obj1;
  root->member2 = obj2;
  obj1->member1 = obj2;
  obj1->member2 = obj3;
  obj3->member1 = root;
  dead->member1 = root;

  RefPtr<TestObject> handle(root);

  RunGc();
  EXPECT_FALSE(is_free_root);
  EXPECT_FALSE(is_free1);
  EXPECT_FALSE(is_free2);
  EXPECT_FALSE(is_free3);
  EXPECT_TRUE(is_free_dead);

  handle.reset();

  // Should free |root| and all indirect children.
  RunGc();
  EXPECT_TRUE(is_free_root);
  EXPECT_TRUE(is_free1);
  EXPECT_TRUE(is_free2);
  EXPECT_TRUE(is_free3);
}

TEST_F(ObjectTrackerIntegration, FreesIndirectV8Objects) {
  bool is_free_root, is_free1;
  TestObject* root = new TestObject(&is_free_root);
  TestObject* obj = new TestObject(&is_free1);
  root->member1 = root;

  // Create a weak pointer to a V8 object.  This will become empty if the
  // object is destroyed.
  WeakJsPtr<JsObject> v8_object = CreateWeakObject();
  obj->v8_member = v8_object;

  EXPECT_FALSE(is_free_root);
  EXPECT_FALSE(is_free1);
  EXPECT_FALSE(v8_object.empty());

  RunGc();
  EXPECT_TRUE(is_free_root);
  EXPECT_TRUE(is_free1);
  EXPECT_TRUE(v8_object.empty());
}

TEST_F(ObjectTrackerIntegration, AliveIndirectlyThroughJavaScript) {
  // An object is alive because it is held by a JavaScript object that is held
  // by an alive backing object.
  bool is_free_root, is_free_other;
  TestObject* root = new TestObject(&is_free_root);
  TestObject* other = new TestObject(&is_free_other);

  RefPtr<TestObject> handle(root);
  WeakJsPtr<JsObject> v8_object = CreateWeakObject();
  root->v8_member = v8_object;

  SetMember(v8_object, "key", other);
  // -> root -> v8_object -> other

  // Because we have |handle| all the objects should remain alive.
  RunGc();
  EXPECT_FALSE(is_free_root);
  EXPECT_FALSE(is_free_other);

  // Clear |handle| and ensure all the objects are destroyed.
  handle.reset();
  RunGc();
  EXPECT_TRUE(is_free_root);
  EXPECT_TRUE(is_free_other);
}

TEST_F(ObjectTrackerIntegration, ComplexReferences) {
  // A complex network of JavaScript and backing objects referencing each other.
  bool is_free_root, is_free_a, is_free_b, is_free_c, is_free_d, is_free_e;
  bool is_free_dead;
  TestObject* root = new TestObject(&is_free_root);
  TestObject* A = new TestObject(&is_free_a);
  TestObject* B = new TestObject(&is_free_b);
  TestObject* C = new TestObject(&is_free_c);
  TestObject* D = new TestObject(&is_free_d);
  TestObject* E = new TestObject(&is_free_e);
  TestObject* dead = new TestObject(&is_free_dead);

  WeakJsPtr<JsObject> W = CreateWeakObject();
  WeakJsPtr<JsObject> X = CreateWeakObject();
  WeakJsPtr<JsObject> Y = CreateWeakObject();
  WeakJsPtr<JsObject> Z = CreateWeakObject();
  WeakJsPtr<JsObject> v8_dead = CreateWeakObject();

  RefPtr<TestObject> handle(root);

  {
    root->member1 = A;
    root->member2 = B;
    A->member1 = B;
    A->member2 = C;
    B->v8_member = W;
    SetMember(W, "mem", C);
    SetMember(W, "mem2", X);
    SetMember(X, "mem", D);
    D->v8_member = Y;
    SetMember(Y, "mem", E);
    SetMember(Y, "mem2", root);
    E->member1 = D;
    E->v8_member = Z;

    dead->v8_member = v8_dead;
  }

  RunGc();
  EXPECT_FALSE(is_free_root);
  EXPECT_FALSE(is_free_a);
  EXPECT_FALSE(is_free_b);
  EXPECT_FALSE(is_free_c);
  EXPECT_FALSE(is_free_d);
  EXPECT_FALSE(is_free_e);
  EXPECT_TRUE(is_free_dead);
  EXPECT_FALSE(W.empty());
  EXPECT_FALSE(X.empty());
  EXPECT_FALSE(Y.empty());
  EXPECT_FALSE(Z.empty());
  EXPECT_TRUE(v8_dead.empty());

  handle.reset();
  RunGc();
  EXPECT_TRUE(is_free_root);
  EXPECT_TRUE(is_free_a);
  EXPECT_TRUE(is_free_b);
  EXPECT_TRUE(is_free_c);
  EXPECT_TRUE(is_free_d);
  EXPECT_TRUE(is_free_e);
  EXPECT_TRUE(is_free_dead);
  EXPECT_TRUE(W.empty());
  EXPECT_TRUE(X.empty());
  EXPECT_TRUE(Y.empty());
  EXPECT_TRUE(Z.empty());
  EXPECT_TRUE(v8_dead.empty());
}

TEST_F(ObjectTrackerIntegration, SupportsMoveWhileRunning) {
  bool is_free_dest, is_free_middle, is_free_source, is_free_extra;
  TestObject* dest = new TestObject(&is_free_dest);
  TestObject* middle = new TestObject(&is_free_middle);
  TestObject* source = new TestObject(&is_free_source);
  TestObject* extra = new TestObject(&is_free_extra);

  WeakJsPtr<JsObject> dest_v8 = CreateWeakObject();
  WeakJsPtr<JsObject> middle_v8 = CreateWeakObject();
  WeakJsPtr<JsObject> source_v8 = CreateWeakObject();

  RefPtr<TestObject> handle(dest);
  {
    dest->v8_member = dest_v8;
    SetMember(dest_v8, "abc", middle);
    middle->v8_member = middle_v8;
    SetMember(middle_v8, "abc", source);
    source->member1 = extra;
  }

  middle->on_trace = [&]() {
    // We should have already traced |dest|, but |source| should not have been
    // traced.  This will move |member1| from |source| to |dest|.  This would
    // normally cause a memory leak since |dest| has already been traced so we
    // would lose the member.
    //
    // This is also testing a number of related bugs such as allocating new
    // objects on background threads.  This is important since JavaScript
    // can run between GC steps.  However, this is probably unlikely to
    // happen since JavaScript references are handled by V8, so this will
    // only happen if our code does something like this.
    EXPECT_FALSE(source->has_been_traced());
    EXPECT_TRUE(dest->has_been_traced());
    dest->member1 = std::move(source->member1);
  };
  RunGc();

  EXPECT_FALSE(is_free_dest);
  EXPECT_FALSE(is_free_middle);
  EXPECT_FALSE(is_free_source);
  EXPECT_FALSE(is_free_extra);

  handle.reset();
  middle->on_trace = &Noop;
  RunGc();
}

TEST_F(ObjectTrackerIntegration, SupportsCreatingNewObjects) {
  bool is_free_first, is_free_creator, is_free_createe = false;
  new TestObject(&is_free_first);
  TestObject* createe = nullptr;
  TestObject* creator = new TestObject(&is_free_creator);

  creator->on_destroy = [&]() { createe = new TestObject(&is_free_createe); };

  RunGc();

  EXPECT_TRUE(is_free_first);
  EXPECT_TRUE(is_free_creator);
  EXPECT_TRUE(createe);
  EXPECT_FALSE(is_free_createe);

  RunGc();

  EXPECT_TRUE(is_free_createe);
}

TEST_F(ObjectTrackerIntegration, CanReusePointers) {
  bool is_free_first;
  int free_count = 0;
  std::array<uint8_t, sizeof(ReusableTestObject)> memory;
  // Initialize the existing memory with the TestObject, this should register
  // the pointer with the object tracker.
  new (memory.data()) ReusableTestObject(&free_count);
  TestObject* first = new TestObject(&is_free_first);

  first->on_destroy = [&]() {
    // The object should be freed already.  Initialize the same memory again
    // with a new object, which should re-register the same pointer.
    ASSERT_EQ(free_count, 1u);
    new (memory.data()) ReusableTestObject(&free_count);
  };

  RunGc();

  EXPECT_TRUE(is_free_first);
  EXPECT_EQ(free_count, 1u);

  RunGc();

  EXPECT_EQ(free_count, 2u);
}

TEST_F(ObjectTrackerIntegration, CanReuseObjectsInDispose) {
  bool is_free_first;
  int free_count = 0;
  std::array<uint8_t, sizeof(ReusableTestObject)> memory;
  // Initialize the existing memory with the TestObject, this should register
  // the pointer with the object tracker.
  new (memory.data()) ReusableTestObject(&free_count);
  TestObject* first = new TestObject(&is_free_first);

  first->on_destroy = [&]() {
    // The object should be freed already.  Initialize the same memory again
    // with a new object, which should re-register the same pointer.
    ASSERT_EQ(free_count, 1u);
    new (memory.data()) ReusableTestObject(&free_count);
  };

  tracker_.Dispose();

  EXPECT_TRUE(is_free_first);
  EXPECT_EQ(free_count, 2u);
}

}  // namespace memory
}  // namespace shaka

#endif  // USING_V8
