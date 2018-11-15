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

#include "src/mapping/js_wrappers.h"

#include <fstream>

#include "src/mapping/backing_object.h"
#include "src/mapping/convert_js.h"
#include "src/mapping/js_utils.h"
#include "src/util/file_system.h"

namespace shaka {

namespace {

JSClassDefinition wrapper_class_def = {
    .className = "<pointer wrapper>",
    .version = 1,
};

JSClassRef GetWrapperClass() {
  static JSClassRef class_ = nullptr;
  if (!class_) {
    class_ = JSClassCreate(&wrapper_class_def);
    CHECK(class_);
  }

  return class_;
}

bool IsInstanceOfStandardType(Handle<JsValue> value, const std::string& type) {
  JSContextRef cx = GetContext();
  JSValueRef ctor = GetMemberRaw(JSContextGetGlobalObject(cx), type);
  return JSValueIsInstanceOfConstructor(cx, value, UnsafeJsCast<JsObject>(ctor),
                                        nullptr);
}

}  // namespace

namespace util {

template <>
struct RefTypeTraits<JSPropertyNameArrayRef> {
  static constexpr const bool AcquireWithRaw = false;

  static JSPropertyNameArrayRef Duplicate(JSPropertyNameArrayRef arg) {
    if (arg)
      return JSPropertyNameArrayRetain(arg);
    else
      return nullptr;
  }

  static void Release(JSPropertyNameArrayRef arg) {
    if (arg)
      JSPropertyNameArrayRelease(arg);
  }
};

}  // namespace util

// \cond Doxygen_Skip

CallbackArguments::CallbackArguments(const JSValueRef* args, size_t count,
                                     JSObjectRef callee, JSObjectRef thisv,
                                     JSValueRef* except)
    : except_(except),
      callee_(callee),
      this_(thisv),
      args_(args),
      count_(count) {}
CallbackArguments::~CallbackArguments() {}

ReturnVal<JsValue> CallbackArguments::operator[](size_t i) const {
  if (i >= count_)
    return nullptr;
  return args_[i];
}

void CallbackArguments::SetReturn(Handle<JsValue> ret) const {
  DCHECK(!ret_);
  ret_ = ret;
}

void CallbackArguments::SetException(Handle<JsValue> except) const {
  DCHECK(!ret_);
  *except_ = except;
}


std::vector<std::string> GetMemberNames(Handle<JsObject> object) {
  auto* cx = GetContext();
  util::CFRef<JSPropertyNameArrayRef> props =
      JSObjectCopyPropertyNames(cx, object);
  const size_t max = JSPropertyNameArrayGetCount(props);

  std::vector<std::string> ret;
  ret.reserve(max);
  for (size_t i = 0; i < max; i++) {
    LocalVar<JsString> name(JSPropertyNameArrayGetNameAtIndex(props, i));
    ret.emplace_back(ConvertToString(RawToJsValue(name)));
  }
  return ret;
}

ReturnVal<JsValue> GetMemberRaw(Handle<JsObject> object,
                                const std::string& name) {
  return JSObjectGetProperty(GetContext(), object, JsStringFromUtf8(name),
                             nullptr);
}

ReturnVal<JsValue> GetArrayIndexRaw(Handle<JsObject> object, size_t index) {
  return JSObjectGetPropertyAtIndex(GetContext(), object, index, nullptr);
}

void SetMemberRaw(Handle<JsObject> object, const std::string& name,
                  Handle<JsValue> value) {
  JSObjectSetProperty(GetContext(), object, JsStringFromUtf8(name), value,
                      kJSPropertyAttributeNone, nullptr);
}

void SetArrayIndexRaw(Handle<JsObject> object, size_t i,
                      Handle<JsValue> value) {
  JSObjectSetPropertyAtIndex(GetContext(), object, i, value, nullptr);
}

void SetGenericPropertyRaw(Handle<JsObject> object, const std::string& name,
                           Handle<JsFunction> getter,
                           Handle<JsFunction> setter) {
  // TODO: Find a better way to do this.
  // This works by effectively running the following JavaScript:
  //   Object.defineProperty($object, $name, {get: $getter, set: $setter});
  LocalVar<JsValue> js_Object =
      GetMemberRaw(JSContextGetGlobalObject(GetContext()), "Object");
  CHECK(js_Object && IsObject(js_Object));
  LocalVar<JsValue> define_prop =
      GetMemberRaw(UnsafeJsCast<JsObject>(js_Object), "defineProperty");
  CHECK(define_prop && GetValueType(define_prop) == JSValueType::Function);

  LocalVar<JsObject> props(CreateObject());
  SetMemberRaw(props, "get", getter);
  if (setter)
    SetMemberRaw(props, "set", setter);

  LocalVar<JsValue> args[] = {object, ToJsValue(name), props};
  LocalVar<JsValue> except;
  CHECK(InvokeMethod(UnsafeJsCast<JsObject>(define_prop),
                     UnsafeJsCast<JsObject>(js_Object), 3, args, &except))
      << ConvertToString(except);
}

bool InvokeConstructor(Handle<JsFunction> ctor, int argc,
                       LocalVar<JsValue>* argv,
                       LocalVar<JsValue>* result_or_except) {
  JSValueRef except = nullptr;
  std::vector<JSValueRef> args(argv, argv + argc);
  LocalVar<JsValue> ret =
      JSObjectCallAsConstructor(GetContext(), ctor, argc, args.data(), &except);
  if (except) {
    *result_or_except = except;
    return false;
  } else {
    *result_or_except = ret;
    return true;
  }
}

bool InvokeMethod(Handle<JsFunction> func, Handle<JsObject> that, int argc,
                  LocalVar<JsValue>* argv,
                  LocalVar<JsValue>* result_or_except) {
  JSValueRef except = nullptr;
  std::vector<JSValueRef> args(argv, argv + argc);
  LocalVar<JsValue> ret = JSObjectCallAsFunction(GetContext(), func, that, argc,
                                                 args.data(), &except);
  if (except) {
    *result_or_except = except;
    return false;
  } else {
    *result_or_except = ret;
    return true;
  }
}

std::string ConvertToString(Handle<JsValue> value) {
  LocalVar<JsString> str(JSValueToStringCopy(GetContext(), value, nullptr));
  if (!str)
    return "";

  const size_t max_size = JSStringGetMaximumUTF8CStringSize(str);
  std::string ret(max_size, '\0');
  const size_t real_size = JSStringGetUTF8CString(str, &ret[0], ret.size());
  ret.resize(real_size - 1);  // Subtract 1 to remove the null-terminator.
  return ret;
}

ReturnVal<JsValue> WrapPointer(void* ptr) {
  return JSObjectMake(GetContext(), GetWrapperClass(), ptr);
}

void* MaybeUnwrapPointer(Handle<JsValue> value) {
  if (!JSValueIsObjectOfClass(GetContext(), value, GetWrapperClass()))
    return nullptr;
  return JSObjectGetPrivate(JSValueToObject(GetContext(), value, nullptr));
}

BackingObject* GetInternalPointer(Handle<JsValue> value) {
  LocalVar<JsObject> obj(JSValueToObject(GetContext(), value, nullptr));
  if (!obj)
    return nullptr;
  return reinterpret_cast<BackingObject*>(JSObjectGetPrivate(obj));
}

bool IsDerivedFrom(BackingObject* ptr, const std::string& name) {
  return ptr ? ptr->DerivedFrom(name) : false;
}

bool RunScript(const std::string& path) {
  util::FileSystem fs;
  std::vector<uint8_t> code;
  CHECK(fs.ReadFile(path, &code));
  return RunScript(path, code.data(), code.size());
}

bool RunScript(const std::string& path, const uint8_t* data, size_t size) {
  const std::vector<uint16_t> data_utf16(data, data + size);
  LocalVar<JsString> code =
      JSStringCreateWithCharacters(data_utf16.data(), data_utf16.size());
  LocalVar<JsString> source = JsStringFromUtf8(path);

  JSValueRef except = nullptr;
  (void)JSEvaluateScript(GetContext(), code, nullptr, source, 0, &except);
  if (except) {
    OnUncaughtException(except, false);
    return false;
  }
  return true;
}

ReturnVal<JsValue> ParseJsonString(const std::string& json) {
  LocalVar<JsString> input = JsStringFromUtf8(json);
  return JSValueMakeFromJSONString(GetContext(), input);
}


ReturnVal<JsString> JsStringFromUtf8(const std::string& str) {
  util::CFRef<CFStringRef> cf_str(CFStringCreateWithBytes(
      nullptr, reinterpret_cast<const uint8_t*>(str.data()), str.size(),
      kCFStringEncodingUTF8, false));
  return JSStringCreateWithCFString(cf_str);
}

ReturnVal<JsValue> JsUndefined() {
  return JSValueMakeUndefined(GetContext());
}

ReturnVal<JsValue> JsNull() {
  return JSValueMakeNull(GetContext());
}

ReturnVal<JsObject> CreateArray(size_t length) {
  JSContextRef cx = GetContext();
  LocalVar<JsObject> arr(JSObjectMakeArray(cx, 0, nullptr, nullptr));
  SetMemberRaw(arr, "length", JSValueMakeNumber(cx, length));
  return arr;
}

ReturnVal<JsObject> CreateObject() {
  return JSObjectMake(GetContext(), nullptr, nullptr);
}

ReturnVal<JsMap> CreateMap() {
  JSContextRef cx = GetContext();
  LocalVar<JsObject> global = JSContextGetGlobalObject(cx);
  JSValueRef ctor = GetMemberRaw(global, "Map");
  DCHECK(ctor && IsObject(ctor));
  LocalVar<JsObject> ctor_obj = UnsafeJsCast<JsObject>(ctor);

  LocalVar<JsValue> ret;
  CHECK(InvokeConstructor(ctor_obj, 0, nullptr, &ret));
  return UnsafeJsCast<JsObject>(ret);
}

void SetMapValue(Handle<JsMap> map, Handle<JsValue> key,
                 Handle<JsValue> value) {
  LocalVar<JsValue> func_value = GetMemberRaw(map, "set");
  DCHECK(GetValueType(func_value) == JSValueType::Function);
  LocalVar<JsFunction> func = UnsafeJsCast<JsFunction>(func_value);

  LocalVar<JsValue> args[] = {key, value};
  LocalVar<JsValue> ignored;
  CHECK(InvokeMethod(func, map, 2, args, &ignored));
}

bool IsNullOrUndefined(Handle<JsValue> value) {
  JSContextRef cx = GetContext();
  return !value || JSValueIsNull(cx, value) || JSValueIsUndefined(cx, value);
}

bool IsObject(Handle<JsValue> value) {
  return JSValueIsObject(GetContext(), value);
}

bool IsBuiltInObject(Handle<JsObject> object) {
  LocalVar<JsValue> to_string_val =
      GetDescendant(JSContextGetGlobalObject(GetContext()),
                    {"Object", "prototype", "toString"});
  CHECK(IsObject(to_string_val));

  LocalVar<JsObject> to_string = UnsafeJsCast<JsObject>(to_string_val);
  LocalVar<JsValue> value;
  CHECK(InvokeMethod(to_string, object, 0, nullptr, &value));
  return ConvertToString(value) != "[object Object]";
}

JSValueType GetValueType(Handle<JsValue> value) {
  JSContextRef cx = GetContext();
  switch (JSValueGetType(cx, value)) {
    case kJSTypeUndefined:
      return JSValueType::Undefined;
    case kJSTypeNull:
      return JSValueType::Null;
    case kJSTypeBoolean:
      return JSValueType::Boolean;
    case kJSTypeNumber:
      return JSValueType::Number;
    case kJSTypeString:
      return JSValueType::String;
    case kJSTypeObject:
      break;
    default:
      LOG(FATAL) << "Unknown JavaScript value type";
  }
  // Note JSC doesn't support symbols.

  if (JSValueIsArray(cx, value))
    return JSValueType::Array;
  if (JSObjectIsFunction(cx, JSValueToObject(cx, value, nullptr)))
    return JSValueType::Function;

  switch (JSValueGetTypedArrayType(cx, value, nullptr)) {
    case kJSTypedArrayTypeArrayBuffer:
      return JSValueType::ArrayBuffer;
    case kJSTypedArrayTypeFloat32Array:
      return JSValueType::Float32Array;
    case kJSTypedArrayTypeFloat64Array:
      return JSValueType::Float64Array;
    case kJSTypedArrayTypeInt16Array:
      return JSValueType::Int16Array;
    case kJSTypedArrayTypeInt32Array:
      return JSValueType::Int32Array;
    case kJSTypedArrayTypeInt8Array:
      return JSValueType::Int8Array;
    case kJSTypedArrayTypeUint16Array:
      return JSValueType::Uint16Array;
    case kJSTypedArrayTypeUint32Array:
      return JSValueType::Uint32Array;
    case kJSTypedArrayTypeUint8Array:
      return JSValueType::Uint8Array;
    case kJSTypedArrayTypeUint8ClampedArray:
      return JSValueType::Uint8ClampedArray;

    case kJSTypedArrayTypeNone:
      break;
    default:
      LOG(FATAL) << "Unknown JavaScript TypedArray type";
  }

  if (IsInstanceOfStandardType(value, "Boolean"))
    return JSValueType::BooleanObject;
  if (IsInstanceOfStandardType(value, "String"))
    return JSValueType::StringObject;
  if (IsInstanceOfStandardType(value, "Number"))
    return JSValueType::NumberObject;
  if (IsInstanceOfStandardType(value, "Promise"))
    return JSValueType::Promise;

  return JSValueType::OtherObject;
}

double NumberFromValue(Handle<JsValue> value) {
  JSContextRef cx = GetContext();
  DCHECK(JSValueIsNumber(cx, value) ||
         IsInstanceOfStandardType(value, "Number"));
  return JSValueToNumber(cx, value, nullptr);
}

bool BooleanFromValue(Handle<JsValue> value) {
  JSContextRef cx = GetContext();
  DCHECK(JSValueIsBoolean(cx, value) ||
         IsInstanceOfStandardType(value, "Boolean"));
  return JSValueToBoolean(cx, value);
}

// \endcond Doxygen_Skip

}  // namespace shaka
