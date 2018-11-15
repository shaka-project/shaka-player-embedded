// Copyright 2017 Google LLC
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

#include "src/mapping/backing_object_factory.h"

#include "src/js/js_error.h"

namespace shaka {

namespace {

ExceptionOr<void> NotImplementedCallback() {
  return js::JsError::DOMException(NotSupportedError,
                                   "This feature is not implemented.");
}

#ifdef USING_V8

/**
 * This defines the callback that is called when the object is indexed
 * (i.e. foo[1]).  This forwards to the backing type's indexer method based
 * in the internal data for the call.
 */
struct JsIndexerCallback {
  static void GetIndex(uint32_t index,
                       const v8::PropertyCallbackInfo<v8::Value>& info) {
    v8::HandleScope handle_scope(GetIsolate());
    void* ptr = MaybeUnwrapPointer(info.Data());
    if (!ptr) {
      impl::ThrowError</* ReturnPromise */ false>::General(
          nullptr, "", "", "INTERNAL: Invalid function data.");
      return;
    }
    auto* factory = reinterpret_cast<BackingObjectFactoryBase*>(ptr);
    info.GetReturnValue().Set(factory->GetIndex(info.This(), index));
  }

  static void SetIndex(uint32_t index, v8::Local<v8::Value> given,
                       const v8::PropertyCallbackInfo<v8::Value>& info) {
    v8::HandleScope handle_scope(GetIsolate());
    void* ptr = MaybeUnwrapPointer(info.Data());
    if (!ptr) {
      impl::ThrowError</* ReturnPromise */ false>::General(
          nullptr, "", "", "INTERNAL: Invalid function data.");
      return;
    }
    auto* factory = reinterpret_cast<BackingObjectFactoryBase*>(ptr);
    factory->SetIndex(info.This(), index, given);
  }
};

#elif defined(USING_JSC)

bool TryGetIndex(JSStringRef name, size_t* index) {
  JSContextRef cx = GetContext();
  JSValueRef except = nullptr;
  const double number =
      JSValueToNumber(cx, JSValueMakeString(cx, name), &except);
  if (except || isnan(number))
    return false;
  *index = static_cast<size_t>(number);
  return true;
}

JSValueRef GetProperty(JSContextRef cx, JSObjectRef target, JSStringRef name,
                       JSValueRef* except) {
  // DCHECK_EQ(cx, GetContext());
  auto* object = reinterpret_cast<BackingObject*>(JSObjectGetPrivate(target));
  size_t index;
  if (!TryGetIndex(name, &index))
    return nullptr;
  return object->factory()->GetIndex(target, index);
}

bool SetProperty(JSContextRef cx, JSObjectRef target, JSStringRef name,
                 JSValueRef given, JSValueRef* except) {
  // DCHECK_EQ(cx, GetContext());
  auto* object = reinterpret_cast<BackingObject*>(JSObjectGetPrivate(target));
  size_t index;
  if (!TryGetIndex(name, &index))
    return false;
  object->factory()->SetIndex(target, index, given);
  return true;
}

#endif

}  // namespace

BackingObjectFactoryBase::BackingObjectFactoryBase(
    const std::string& name, NativeCtor ctor,
    const BackingObjectFactoryBase* base)
    : type_name_(name), base_(base) {
#ifdef USING_V8
  v8::Isolate* isolate = GetIsolate();

  const v8::Local<v8::Signature> empty_signature;
  v8::Local<v8::FunctionTemplate> type = v8::FunctionTemplate::New(
      isolate, ctor, v8::Local<v8::Value>(), empty_signature, 0);
  type->SetClassName(JsStringFromUtf8(type_name_));
  // We will store the pointer to the object in field 0.  Field 1 must contain
  // a valid pointer so the V8 GC will pass it to
  // EmbedderHeapTracer::RegisterV8References.
  // See: v8/src/heap/mark_compact.cc:2228.
  type->InstanceTemplate()->SetInternalFieldCount(
      BackingObject::kInternalFieldCount);
  if (base)
    type->Inherit(base->class_definition_);

  // Add a callback for when the object is indexed.
  v8::IndexedPropertyGetterCallback getter = &JsIndexerCallback::GetIndex;
  v8::IndexedPropertySetterCallback setter = &JsIndexerCallback::SetIndex;
  type->PrototypeTemplate()->SetIndexedPropertyHandler(
      getter, setter, nullptr, nullptr, nullptr,
      WrapPointer(static_cast<BackingObjectFactoryBase*>(this)));

  v8::MaybeLocal<v8::Function> maybe_ctor =
      type->GetFunction(isolate->GetCurrentContext());
  constructor_ = maybe_ctor.ToLocalChecked();
  class_definition_ = type;
  prototype_ = UnsafeJsCast<JsObject>(GetMemberRaw(constructor_, "prototype"));

  // Register the type on 'window'.
  SetMemberRaw(JsEngine::Instance()->global_handle(), type_name_, constructor_);
#elif defined(USING_JSC)
  memset(&definition_, 0, sizeof(definition_));
  definition_.className = type_name_.c_str();
  definition_.version = 1;
  definition_.getProperty = &GetProperty;
  definition_.setProperty = &SetProperty;
  if (base)
    definition_.parentClass = base->GetClass();

  JSContextRef cx = GetContext();
  class_definition_ = JSClassCreate(&definition_);
  constructor_ = JSObjectMakeConstructor(cx, class_definition_, ctor);
  prototype_ = UnsafeJsCast<JsObject>(GetMemberRaw(constructor_, "prototype"));

  // Register the type on the global object.
  SetMemberRaw(JSContextGetGlobalObject(cx), type_name_, constructor_);
#endif
}

BackingObjectFactoryBase::~BackingObjectFactoryBase() {}

bool BackingObjectFactoryBase::DerivedFrom(const std::string& name) const {
  if (name == type_name_)
    return true;
  if (!base_)
    return name == "BackingObject";
  return base_->DerivedFrom(name);
}

ReturnVal<JsValue> BackingObjectFactoryBase::WrapInstance(
    BackingObject* object) {
  LocalVar<JsValue> result;
  LocalVar<JsValue> args[] = {WrapPointer(object)};
  CHECK(InvokeConstructor(constructor_, 1, args, &result));
  return result;
}

ReturnVal<JsValue> BackingObjectFactoryBase::GetIndex(Handle<JsObject> that,
                                                      size_t index) {
  if (!indexer_)
    return JsUndefined();
  return indexer_->GetIndex(that, index);
}

void BackingObjectFactoryBase::SetIndex(Handle<JsObject> that, size_t index,
                                        Handle<JsValue> value) {
  if (indexer_)
    indexer_->SetIndex(that, index, value);
}

void BackingObjectFactoryBase::NotImplemented(const std::string& name) {
  LocalVar<JsFunction> getter = CreateStaticFunction(
      type_name_, name,
      std::function<ExceptionOr<void>()>(&NotImplementedCallback));
  SetGenericPropertyRaw(prototype_, name, getter, getter);
}

}  // namespace shaka
