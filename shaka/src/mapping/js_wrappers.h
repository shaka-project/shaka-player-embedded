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

#ifndef SHAKA_EMBEDDED_MAPPING_JS_WRAPPERS_H_
#define SHAKA_EMBEDDED_MAPPING_JS_WRAPPERS_H_

#include <glog/logging.h>

#include <string>
#include <vector>

#include "src/util/macros.h"
// Use the IndexedDB protobuf for ValueType so we don't have to duplicate
// the enum here and for IndexedDB storage.
#include "src/js/idb/database.pb.h"

#if defined(USING_V8)
#  include <v8.h>
#  include "src/mapping/v8/v8_utils.h"
#elif defined(USING_JSC)
#  include <JavaScriptCore/JavaScriptCore.h>
#  include "src/mapping/jsc/jsc_utils.h"
#  include "src/util/cfref.h"
#endif

namespace shaka {

class BackingObject;

namespace util {

#ifdef USING_JSC
template <>
struct RefTypeTraits<JSStringRef> {
  static constexpr const bool AcquireWithRaw = true;

  static JSStringRef Duplicate(JSStringRef arg) {
    if (arg)
      JSStringRetain(arg);
    return arg;
  }

  static void Release(JSStringRef arg) {
    if (arg)
      JSStringRelease(arg);
  }
};
template <>
struct RefTypeTraits<JSValueRef> {
  static constexpr const bool AcquireWithRaw = true;

  static JSValueRef Duplicate(JSValueRef arg) {
    if (arg)
      JSValueProtect(GetContext(), arg);
    return arg;
  }

  static void Release(JSValueRef arg) {
    if (arg)
      JSValueUnprotect(GetContext(), arg);
  }
};
template <>
struct RefTypeTraits<JSObjectRef> : RefTypeTraits<JSValueRef> {
  static JSObjectRef Duplicate(JSObjectRef arg) {
    if (arg)
      JSValueProtect(GetContext(), arg);
    return arg;
  }
};

#endif

}  // namespace util

/**
 * This defines several types that are used to represent JavaScript values.
 *
 * Handle<T> represents a handle to a JavaScript value.
 * LocalVar<T> represents a local variable that holds a JavaScript value.
 * ReturnVal<T> is a JavaScript value that is returned from a C++ function.
 *   This is NOT a Handle<T>.
 *
 * For compatibility, you should not pass a ReturnVal<T> as an argument.  Store
 * it in a LocalVar<T> first before passing.  Also, different types should be
 * incompatible with each other (i.e. a JsString is not a JsValue).
 */


#if defined(USING_V8)
template <typename T>
using Handle = v8::Local<T>;
template <typename T>
using LocalVar = v8::Local<T>;
template <typename T>
using ReturnVal = v8::Local<T>;
template <typename T>
struct Global {
  Global() {}
  template <typename U = T>
  explicit Global(v8::Local<U> val) : val_(GetIsolate(), val) {}

  template <typename U = T>
  operator v8::Local<U>() const {
    return val_.Get(GetIsolate());
  }
  operator bool() const {
    return !val_.IsEmpty();
  }

  template <typename U = T>
  Global& operator=(v8::Local<U> other) {
    val_.Reset(GetIsolate(), other);
    return *this;
  }
  Global& operator=(std::nullptr_t) {
    val_.Reset();
    return *this;
  }

 private:
  v8::Global<T> val_;
};

using JsValue = v8::Value;
using JsObject = v8::Object;
using JsString = v8::String;
using JsFunction = v8::Function;
using JsPromise = v8::Promise;
using JsMap = v8::Map;

/**
 * Defines the arguments to a JavaScript function.  This includes the positional
 * arguments passed in as well as |this|.  This also includes a special field
 * for setting the return value of the function.
 */
using CallbackArguments = v8::FunctionCallbackInfo<v8::Value>;
#elif defined(USING_JSC)
template <typename T>
using Handle = util::CFRef<T>;
template <typename T>
using LocalVar = Handle<T>;
template <typename T>
using ReturnVal = Handle<T>;
template <typename T>
using Global = Handle<T>;

using JsValue = JSValueRef;
using JsObject = JSObjectRef;
using JsString = JSStringRef;
using JsFunction = JSObjectRef;
using JsPromise = JSObjectRef;
using JsMap = JSObjectRef;

class CallbackArguments {
 public:
  CallbackArguments(const JSValueRef* args, size_t count, JSObjectRef callee,
                    JSObjectRef thisv, JSValueRef* except);
  ~CallbackArguments();

  size_t length() const {
    return count_;
  }
  JSObjectRef callee() const {
    return callee_;
  }
  JSObjectRef thisv() const {
    return this_;
  }
  ReturnVal<JSValueRef> ret() const {
    return ret_;
  }

  ReturnVal<JsValue> operator[](size_t i) const;

  void SetReturn(Handle<JsValue> ret) const;
  void SetException(Handle<JsValue> except) const;

 private:
  // These need to be modifiable within const methods since in V8
  // we can modify the return value when the arguments are const.
  mutable JSValueRef* except_;
  mutable Handle<JsValue> ret_;
  const JSObjectRef callee_;
  const JSObjectRef this_;
  const JSValueRef* args_;
  const size_t count_;
};
#endif


/** @return The number of arguments that were given. */
inline size_t ArgumentCount(const CallbackArguments& arguments) {
#if defined(USING_V8)
  return arguments.Length();
#elif defined(USING_JSC)
  return arguments.length();
#endif
}

/**
 * Get the properties of the current object. This will only return the
 * properties on 'this' and not on the prototype.
 *
 * @param object The object of interest.
 * @return The member names found in the given object.
 */
std::vector<std::string> GetMemberNames(Handle<JsObject> object);

/** @return The given member of the given object. */
ReturnVal<JsValue> GetMemberRaw(Handle<JsObject> object,
                                const std::string& name,
                                LocalVar<JsValue>* exception = nullptr);

/** @return The member at the given index of the given object. */
ReturnVal<JsValue> GetArrayIndexRaw(Handle<JsObject> object, size_t index,
                                    LocalVar<JsValue>* exception = nullptr);

/** Sets the given member on the given object. */
void SetMemberRaw(Handle<JsObject> object, const std::string& name,
                  Handle<JsValue> value);

/** Sets the member at the given index of the given object. */
void SetArrayIndexRaw(Handle<JsObject> object, size_t i, Handle<JsValue> value);

/** Adds a generic property on the given object. */
void SetGenericPropertyRaw(Handle<JsObject> object, const std::string& name,
                           Handle<JsFunction> getter,
                           Handle<JsFunction> setter);


/**
 * Calls the given function as a constructor and gives the resulting object
 * or the exception that was thrown.
 * @param ctor The constructor to call.
 * @param argc The number of arguments to pass in.
 * @param argv The array of arguments to pass in.
 * @param result_or_except [OUT] Will contain the resulting object or the
 *   exception that was thrown.
 * @return True if the constructor returned an object, false if it threw an
 *   exception.
 */
bool InvokeConstructor(Handle<JsFunction> ctor, int argc,
                       LocalVar<JsValue>* argv,
                       LocalVar<JsValue>* result_or_except);

/**
 * Calls the given function and gives the resulting return value or the
 * exception that was thrown.
 * @param method The method to call.
 * @param that The value that should be |this| in the call.
 * @param argc The number of arguments to pass in.
 * @param argv The array of arguments to pass in.
 * @param result_or_except [OUT] Will contain the resulting object or the
 *   exception that was thrown.
 * @return True if the function returned an object, false if it threw an
 *   exception.
 */
bool InvokeMethod(Handle<JsFunction> method, Handle<JsObject> that, int argc,
                  LocalVar<JsValue>* argv, LocalVar<JsValue>* result_or_except);


/** @return The given value converted to a string. */
std::string ConvertToString(Handle<JsValue> value);

/** @return A new JavaScript object that wraps the given pointer. */
ReturnVal<JsValue> WrapPointer(void* ptr);

/**
 * @return The pointer the given value wraps, or nullptr if not a wrapped
 *   pointer.
 */
void* MaybeUnwrapPointer(Handle<JsValue> value);

/**
 * @return The internal pointer for the given value, or nullptr if it is not
 *   a backing object.
 */
BackingObject* GetInternalPointer(Handle<JsValue> value);

/**
 * Gets whether the given object derives from the given type name.  This exists
 * so RefPtr<T> can check this without needing the BackingObject header.
 */
bool IsDerivedFrom(BackingObject* ptr, const std::string& name);

/**
 * Reads a JavaScript file from the given path and executes it in the current
 * isolate.
 *
 * @param path The file path to the JavaScript file.
 */
bool RunScript(const std::string& path);

/**
 * Executes the given JavaScript code in the current isolate.
 *
 * @param path The file path to the JavaScript file.  This is only used for
 *   logging.
 * @param data The contents of the file.  It must only contain ASCII and must
 *   live for the duration of the current isolate.
 * @param data_size The number of bytes in |data|.
 */
bool RunScript(const std::string& path, const uint8_t* data, size_t data_size);

/**
 * Parses the given string as JSON and returns the given value.
 * @param json The input string.
 * @return The resulting JavaScrpt value, or nullptr on error.
 */
ReturnVal<JsValue> ParseJsonString(const std::string& json);


/** @return A new string object containing the given UTF-8 string. */
ReturnVal<JsString> JsStringFromUtf8(const std::string& str);

/** @return The JavaScript value |undefined|. */
ReturnVal<JsValue> JsUndefined();

/** @return The JavaScript value |null|. */
ReturnVal<JsValue> JsNull();

/** @return A new JavaScript array object. */
ReturnVal<JsObject> CreateArray(size_t length);

/** @return A new plain JavaScript object. */
ReturnVal<JsObject> CreateObject();

/** @return A new JavaScript Map object. */
ReturnVal<JsMap> CreateMap();

/**
 * Sets the value of the given key in the given map.  This is not the same as
 * SetMemberRaw.
 */
void SetMapValue(Handle<JsMap> map, Handle<JsValue> key, Handle<JsValue> value);

/** @return Whether the given value is |null| or |undefined|. */
bool IsNullOrUndefined(Handle<JsValue> value);

/**
 * @return Whether the given value is an object.  Unlike typeof, this will
 *   return false for |null|.
 */
bool IsObject(Handle<JsValue> value);

/**
 * @return Whether the given object is an instance of a built-in type.  This
 *   includes both JavaScript-defined types like ArrayBuffer and types defined
 *   by BackingObjects.
 */
bool IsBuiltInObject(Handle<JsObject> object);

/** @return The type of value contained. */
proto::ValueType GetValueType(Handle<JsValue> value);


///@{
/**
 * Converts the given value to a different type.  This only performs debug
 * assertions that the type is correct, the caller should check the type
 * beforehand (e.g. by using GetValueType).
 */
template <typename Dest>
ReturnVal<Dest> UnsafeJsCast(Handle<JsValue> source) {
#ifdef USING_V8
  return source.As<Dest>();
#else
  static_assert(std::is_same<Dest, JsValue>::value,
                "Should use other specializations");
  return source;
#endif
}

#ifdef USING_JSC
template <>
inline ReturnVal<JsObject> UnsafeJsCast<JsObject>(Handle<JsValue> source) {
  DCHECK(IsObject(source));
  return JSValueToObject(GetContext(), source, nullptr);
}
#endif
///@}


///@{
/** Converts the given value, object, or string into a value type. */
template <typename T>
ReturnVal<JsValue> RawToJsValue(Handle<T> source) {
  return source;
}

#if defined(USING_JSC)
template <>
inline ReturnVal<JsValue> RawToJsValue<JsString>(Handle<JsString> source) {
  return JSValueMakeString(GetContext(), source);
}
#endif
///@}


inline size_t ArrayLength(Handle<JsObject> value) {
#if defined(USING_V8)
  DCHECK(GetValueType(value) == proto::ValueType::Array);
  return value.As<v8::Array>()->Length();
#elif defined(USING_JSC)
  auto* ctx = GetContext();
  LocalVar<JsValue> length(GetMemberRaw(value, "length"));
  CHECK(length && JSValueIsNumber(ctx, length));
  return static_cast<size_t>(JSValueToNumber(ctx, length, nullptr));
#endif
}

/**
 * @return The primitive number from the given JavaScript number.  This must
 *   be a primitive number or a number object.
 */
double NumberFromValue(Handle<JsValue> value);

/**
 * @return The primitive boolean from the given JavaScript boolean.  This must
 *   be a primitive boolean or a boolean object.
 */
bool BooleanFromValue(Handle<JsValue> value);

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_JS_WRAPPERS_H_
