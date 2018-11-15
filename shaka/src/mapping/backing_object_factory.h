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

#ifndef SHAKA_EMBEDDED_MAPPING_BACKING_OBJECT_FACTORY_H_
#define SHAKA_EMBEDDED_MAPPING_BACKING_OBJECT_FACTORY_H_

#include <glog/logging.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "src/core/ref_ptr.h"
#include "src/js/events/event_names.h"
#include "src/mapping/exception_or.h"
#include "src/mapping/js_wrappers.h"
#include "src/mapping/register_member.h"
#include "src/mapping/struct.h"
#include "src/util/pseudo_singleton.h"
#include "src/util/templates.h"

namespace shaka {
class BackingObject;

#ifdef USING_V8
using NativeCtor = void (*)(const v8::FunctionCallbackInfo<v8::Value>&);
#elif defined(USING_JSC)
using NativeCtor = JSObjectRef (*)(JSContextRef, JSObjectRef, size_t,
                                   const JSValueRef*, JSValueRef*);
#endif

namespace impl {

/**
 * A helper class that handles indexing a JavaScript object.  Having this
 * interface allows the factory to store a pointer to this.  This is required
 * since the types of the members are only known inside the AddIndexer method.
 */
class IndexerHandler {
 public:
  virtual ~IndexerHandler() {}
  virtual ReturnVal<JsValue> GetIndex(Handle<JsObject> that, size_t index) = 0;
  virtual void SetIndex(Handle<JsObject> that, size_t index,
                        Handle<JsValue> value) = 0;
};

template <typename This, typename T>
class IndexerHandlerImpl : public IndexerHandler {
 public:
  IndexerHandlerImpl(const std::string& type_name,
                     bool (This::*get)(size_t, T*) const,
                     void (This::*set)(size_t, T))
      : type_name_(type_name), get_(get), set_(set) {}

  ReturnVal<JsValue> GetIndex(Handle<JsObject> that, size_t index) override {
    RefPtr<T> obj;
    if (!obj.TryConvert(that)) {
      ThrowError</* ReturnPromise */ false>::IllegalInvocation(
          nullptr, "indexer", type_name_);
      return JsUndefined();
    }

    T value;
    if (!((obj.get())->*get_)(index, &value))
      return JsUndefined();  // Not found.
    return ToJsValue(value);
  }

  void SetIndex(Handle<JsObject> that, size_t index,
                Handle<JsValue> given) override {
    RefPtr<T> obj;
    if (!obj.TryConvert(that)) {
      ThrowError</* ReturnPromise */ false>::IllegalInvocation(
          nullptr, "indexer", type_name_);
      return;
    }
    if (!set_) {
      ThrowError</* ReturnPromise */ false>::General(
          nullptr, "indexer", type_name_,
          "Indexer on '" + type_name_ + "' is read-only.");
      return;
    }

    T value;
    if (!FromJsValue(given, &value)) {
      ThrowError</* ReturnPromise */ false>::CannotConvert(
          nullptr, "indexer", type_name_, ConvertToString(given),
          TypeName<T>::name());
      return;
    }
    ((obj.get())->*set_)(index, std::move(value));
  }

 private:
  const std::string type_name_;
  bool (This::*get_)(size_t, T*) const;
  void (This::*set_)(size_t, T);
};

}  // namespace impl

namespace util {

#ifdef USING_JSC
template <>
struct RefTypeTraits<JSClassRef> {
  // The API docs for JSClassCreate says it follows the "Create" rule, which
  // suggests we shouldn't have to acquire; however, if we don't, we will
  // occasionally get crashes that appear like the class is getting deleted
  // while we reference it.  Setting this to true seems to workaround it.
  // TODO: Investigate this further and fix or file a bug.
  static constexpr const bool AcquireWithRaw = true;

  static JSClassRef Duplicate(JSClassRef arg) {
    if (arg)
      return JSClassRetain(arg);
    else
      return nullptr;
  }

  static void Release(JSClassRef arg) {
    if (arg)
      JSClassRelease(arg);
  }
};
#endif

}  // namespace util

/**
 * A base type that defines a factory that creates BackingObject instances.
 * This has several helper methods to register member functions and properties.
 *
 * For each type exposed to JavaScript, create one type that inherits from
 * BackingObject that defines members that will be exposed to Javascript and a
 * factory that inherits from BackingObjectFactory<T> for the object type T.
 * The factory constructor should call methods defined here to register the
 * member pointers of the object.
 *
 * The type T must define one namespace item:
 * - A constexpr name field containing the name of the type.
 * - An optional static Create method.
 */
class BackingObjectFactoryBase {
 public:
  virtual ~BackingObjectFactoryBase();

  /** @return The name of the type being generated. */
  const std::string& name() const {
    return type_name_;
  }

  /** @return The base factory for the base type of this object, or nullptr. */
  const BackingObjectFactoryBase* base() const {
    return base_;
  }

  /** @return The value containing the constructor function. */
  ReturnVal<JsValue> GetConstructor() const {
    return RawToJsValue(Handle<JsFunction>(constructor_));
  }

#if defined(USING_JSC)
  JSClassRef GetClass() const {
    return class_definition_;
  }
#endif

  /**
   * @return Whether the objects being generated are derived from the given
   *   type.  Note that this returns true if name() is passed.
   */
  bool DerivedFrom(const std::string& name) const;

  /**
   * Wraps the given backing instance in a JavaScript object.  This is can
   * only be called on the event thread.  The argument is assumed to be
   * of the correct type.
   */
  ReturnVal<JsValue> WrapInstance(BackingObject* object);


  /**
   * This is called when an object created by this factory is indexed.
   * @param that The JavaScript value |this|.
   * @param index The index that was requested.
   * @return The value at that index, or |undefined|.
   */
  ReturnVal<JsValue> GetIndex(Handle<JsObject> that, size_t index);

  /**
   * This is called when an object created by this factory is indexed.
   * @param that The JavaScript value |this|.
   * @param index The index that was requested.
   * @param value The value that was set to.
   */
  void SetIndex(Handle<JsObject> that, size_t index, Handle<JsValue> value);

 protected:
  BackingObjectFactoryBase(const std::string& name, NativeCtor ctor,
                           const BackingObjectFactoryBase* base);

  template <typename Callback>
  void AddMemberFunction(const std::string& name, Callback callback) {
    LocalVar<JsFunction> js_function =
        CreateMemberFunction(type_name_, name, MakeMemFn<void>(callback));
    LocalVar<JsValue> value(RawToJsValue(js_function));
    SetMemberRaw(prototype_, name, value);
  }

  template <typename Ret, typename... Args>
  void AddStaticFunction(const std::string& name, Ret (*callback)(Args...)) {
    LocalVar<JsFunction> js_function = CreateStaticFunction(
        type_name_, name, std::function<Ret(Args...)>(callback));
    LocalVar<JsValue> value(RawToJsValue(js_function));
    SetMemberRaw(constructor_, name, value);
  }

  /**
   * Registers a field that is the on- listener for the given event.  You MUST
   * call AddListenerField in the object's constructor ALSO.
   */
  template <typename Prop>
  void AddListenerField(js::EventType type, Prop member) {
    AddReadWriteProperty("on" + to_string(type), member);
  }

  template <typename This, typename Prop>
  void AddReadOnlyProperty(const std::string& name, Prop This::*member) {
#ifndef ALLOW_STRUCT_FIELDS
    static_assert(
        !std::is_base_of<Struct, typename std::decay<Prop>::type>::value,
        "Cannot store Structs in fields");
#endif

    std::function<Prop&(RefPtr<This>)> getter =
        [=](RefPtr<This> that) -> Prop& { return that.get()->*member; };
    LocalVar<JsFunction> js_getter =
        CreateMemberFunction(type_name_, "get_" + name, getter);
    LocalVar<JsFunction> setter;
    SetGenericPropertyRaw(prototype_, name, js_getter, setter);
  }

  template <typename This, typename Prop>
  void AddReadWriteProperty(const std::string& name, Prop This::*member) {
#ifndef ALLOW_STRUCT_FIELDS
    static_assert(
        !std::is_base_of<Struct, typename std::decay<Prop>::type>::value,
        "Cannot store Structs in fields");
#endif

    std::function<Prop&(RefPtr<This>)> getter =
        [=](RefPtr<This> that) -> Prop& { return that->*member; };
    LocalVar<JsFunction> js_getter =
        CreateMemberFunction(type_name_, "get_" + name, getter);
    std::function<void(RefPtr<This>, Prop)> setter = [=](RefPtr<This> that,
                                                         Prop value) {
      that->*member = std::move(value);
    };
    LocalVar<JsFunction> js_setter =
        CreateMemberFunction(type_name_, "set_" + name, setter);

    SetGenericPropertyRaw(prototype_, name, js_getter, js_setter);
  }

  template <typename Derived = void, typename This = void,
            typename GetProp = void>
  void AddGenericProperty(const std::string& name,
                          GetProp (This::*get)() const) {
    LocalVar<JsFunction> getter = CreateMemberFunction(
        type_name_, "get_" + name, MakeMemFn<Derived>(get));
    LocalVar<JsFunction> setter;
    SetGenericPropertyRaw(prototype_, name, getter, setter);
  }

  template <typename Derived = void, typename This = void,
            typename GetProp = void, typename SetProp = void,
            typename SetPropRet = void>
  void AddGenericProperty(const std::string& name, GetProp (This::*get)() const,
                          SetPropRet (This::*set)(SetProp)) {
    // Use two different types so the setter can accept a const-reference.
    static_assert(util::decay_is_same<GetProp, SetProp>::value,
                  "Getter and setter must use the same type.");
    static_assert(std::is_same<void, SetPropRet>::value ||
                      std::is_same<ExceptionOr<void>, SetPropRet>::value,
                  "Setter can only return void.");
    LocalVar<JsFunction> getter = CreateMemberFunction(
        type_name_, "get_" + name, MakeMemFn<Derived>(get));
    LocalVar<JsFunction> setter = CreateMemberFunction(
        type_name_, "set_" + name, MakeMemFn<Derived>(set));
    SetGenericPropertyRaw(prototype_, name, getter, setter);
  }

  template <typename T>
  void AddConstant(const std::string& name, T value) {
    LocalVar<JsValue> js_value = ToJsValue(value);
    SetMemberRaw(prototype_, name, js_value);
    SetMemberRaw(constructor_, name, js_value);
  }

  template <typename This, typename T>
  void AddIndexer(bool (This::*get)(size_t, T*) const,
                  void (This::*set)(size_t, T) = nullptr) {
    CHECK(!indexer_);
    indexer_.reset(new impl::IndexerHandlerImpl<This, T>(type_name_, get, set));
  }

  void NotImplemented(const std::string& name);

 private:
  template <typename Derived, typename This>
  using get_this = typename std::conditional<std::is_same<Derived, void>::value,
                                             This, Derived>::type;

  template <typename Derived, typename Callback>
  struct MakeMemFnImpl;
  template <typename Derived, typename This, typename Ret, typename... Args>
  struct MakeMemFnImpl<Derived, Ret (This::*)(Args...)> {
    using ArgThis = get_this<Derived, This>;
    using type = std::function<Ret(RefPtr<ArgThis>, Args...)>;
    static type Make(Ret (This::*callback)(Args...)) {
      return [=](RefPtr<ArgThis> that, Args... args) -> Ret {
        return ((that.get())->*(callback))(std::move(args)...);
      };
    }
  };
  template <typename Derived, typename This, typename Ret, typename... Args>
  struct MakeMemFnImpl<Derived, Ret (This::*)(Args...) const> {
    using ArgThis = get_this<Derived, This>;
    using type = std::function<Ret(RefPtr<ArgThis>, Args...)>;
    static type Make(Ret (This::*callback)(Args...) const) {
      return [=](RefPtr<ArgThis> that, Args... args) -> Ret {
        return ((that.get())->*(callback))(std::move(args)...);
      };
    }
  };

  template <typename Derived, typename Callback>
  static typename MakeMemFnImpl<Derived, Callback>::type MakeMemFn(
      Callback callback) {
    return MakeMemFnImpl<Derived, Callback>::Make(callback);
  }

  const std::string type_name_;
  const BackingObjectFactoryBase* const base_;
  std::unique_ptr<impl::IndexerHandler> indexer_;
  Global<JsFunction> constructor_;
  Global<JsObject> prototype_;
#ifdef USING_V8
  Global<v8::FunctionTemplate> class_definition_;
#elif defined(USING_JSC)
  JSClassDefinition definition_;
  util::CFRef<JSClassRef> class_definition_;
#endif
};


/**
 * An intermediate type that allows getting the singleton instance for a given
 * BackingObject type.  The instance can't be stored in BackingObjectFactory
 * since doing so would require knowing the base type.  To be able to be used
 * in this way, it is valid to use the Instance() method when T is void, but it
 * is invalid to create an instance when T is void.
 */
template <typename T>
class BackingObjectFactoryRegistry
    : public BackingObjectFactoryBase,
      private PseudoSingleton<BackingObjectFactoryRegistry<T>> {
 public:
  explicit BackingObjectFactoryRegistry(BackingObjectFactoryBase* base)
      : BackingObjectFactoryBase(T::name(), &impl::JsConstructor<T>::Call,
                                 base) {}

  /**
   * Returns the instance of the factory that will generate objects of type T.
   * Instance() will CHECK for the value not being null.  There is a
   * specialization below for T == void so this will still return null on that
   * case.
   */
  static BackingObjectFactoryBase* CheckedInstance() {
    return BackingObjectFactoryRegistry::Instance();
  }

 private:
  friend class PseudoSingleton<BackingObjectFactoryRegistry<T>>;
};
template <>
inline BackingObjectFactoryBase*
BackingObjectFactoryRegistry<void>::CheckedInstance() {
  return nullptr;
}


template <typename T, typename Base = void>
class BackingObjectFactory : public BackingObjectFactoryRegistry<T> {
 public:
  BackingObjectFactory()
      : BackingObjectFactoryRegistry<T>(
            BackingObjectFactoryRegistry<Base>::CheckedInstance()) {}
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_BACKING_OBJECT_FACTORY_H_
