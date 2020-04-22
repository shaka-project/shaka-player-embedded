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

#ifndef SHAKA_EMBEDDED_MAPPING_REGISTER_MEMBER_H_
#define SHAKA_EMBEDDED_MAPPING_REGISTER_MEMBER_H_

#include <string>
#include <type_traits>
#include <utility>

#include "src/js/js_error.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/convert_js.h"
#include "src/mapping/enum.h"
#include "src/mapping/exception_or.h"
#include "src/mapping/js_engine.h"
#include "src/mapping/js_wrappers.h"
#include "src/mapping/names.h"
#include "src/mapping/promise.h"
#include "src/util/templates.h"
#include "src/util/utils.h"

namespace shaka {
namespace impl {

/**
 * Careful, thar be magic here.  Only read if you are familiar with template
 * metaprogramming.
 *
 * This file defines several functions that are used to convert a C++ function
 * to a V8 function.  A V8 function accepts a single argument that defines
 * the array of arguments, some internal state, and the return value.  This
 * is not convenient with C++ code that accepts positional arguments and returns
 * the value.
 *
 * First, this checks each argument that is passed in.  It tries to convert
 * the argument to the expected type, throwing a JavaScript exception if it
 * cannot do so.  Then it calls the C++ function.  Finally, if needed, converts
 * the return value and passes back to JavaScript.
 *
 * These methods all return a boolean.  They should return |true| when
 * JavaScript returns normally, or |false| if an exception is thrown.
 */


#if defined(USING_V8)
#  define SET_RETURN_VALUE(args, value) (args).GetReturnValue().Set(value)
#  define SET_EXCEPTION(args, except) GetIsolate()->ThrowException(except)
#  define GET_ARG_THIS(args) (static_cast<v8::Local<v8::Value>>((args).This()))
#elif defined(USING_JSC)
#  define SET_EXCEPTION(args, except) (args).SetException(except)
#  define SET_RETURN_VALUE(args, value) (args).SetReturn(value)
#  define GET_ARG_THIS(args) ((args).thisv())
#endif


template <typename T>
using RawType =
    typename std::remove_cv<typename std::remove_reference<T>::type>::type;

/**
 * Defines a number of methods that throw a TypeError in the current Isolate
 * instance.  These are the same error messages given from Chrome.
 *
 * If |ReturnPromise| is true, this will not throw an exception and will instead
 * set the return value of the method to a Promise that has been rejected with
 * the exception.
 */
template <bool ReturnPromise>
struct ThrowError {
  static constexpr const size_t kBufferSize = 4096;

  static bool IllegalInvocation(const CallbackArguments* args,
                                const std::string& func_name,
                                const std::string& target_name) {
    return General(args, func_name, target_name, "Illegal invocation");
  }

  static bool NotEnoughArgs(const CallbackArguments* args,
                            const std::string& name, const std::string& target,
                            size_t required, size_t given) {
    constexpr const char* kFormat =
        "%zu arguments required, but only %zu present.";
    const std::string buffer = util::StringPrintf(kFormat, required, given);
    return General(args, name, target, buffer);
  }

  static bool CannotConvert(const CallbackArguments* args,
                            const std::string& name, const std::string& target,
                            const std::string& given,
                            const std::string& required) {
    constexpr const char* kFormat = "Cannot convert '%s' to '%s'.";
    const std::string buffer =
        util::StringPrintf(kFormat, given.c_str(), required.c_str());
    return General(args, name, target, buffer);
  }

  static bool General(const CallbackArguments* args, const std::string& name,
                      const std::string& target, const std::string& message) {
    constexpr const char* kFormat = "Failed to execute '%s' on '%s': %s";
    const std::string buffer = util::StringPrintf(
        kFormat, name.c_str(), target.c_str(), message.c_str());
    return Raw(args, js::JsError::TypeError(buffer));
  }

  static bool Raw(const CallbackArguments* args, const js::JsError& value) {
    if (ReturnPromise) {
      Promise ret = Promise::Rejected(value);
      SET_RETURN_VALUE(*args, ret.ToJsValue());
      return true;
    }

    LocalVar<JsValue> except = value.error();
    SET_EXCEPTION(*args, except);
    return false;
  }
};

/**
 * This is called when defining a JavaScript constructor.  This sets up the
 * resulting JavaScript object and links it to the given backing object.
 *
 * In V8, the JavaScript object is already created for us (stored in
 * |arguments.This()|).  All we need to do is connect it to the backing object
 * by setting its internal fields.
 *
 * In JSC, this creates a new JavaScript object using |This| to get the
 * correct type.  This method then sets the return value of the JavaScript
 * function so the object is passed to JavaScript.
 *
 * @param arguments The arguments (including |this| and the return value) to the
 *   JavaScript function.
 * @param that The backing object to wrap.
 */
bool ConstructWrapperObject(const CallbackArguments& arguments,
                            BackingObject* that);


template <typename Ret>
struct HandleSetReturn {
  static bool SetReturn(const CallbackArguments& arguments, const Ret& ret) {
    SET_RETURN_VALUE(arguments, ToJsValue(ret));
    return true;
  }
};
template <>
struct HandleSetReturn<ExceptionOr<void>> {
  static bool SetReturn(const CallbackArguments& arguments,
                        const ExceptionOr<void>& ret) {
    if (holds_alternative<js::JsError>(ret)) {
      LocalVar<JsValue> except = get<js::JsError>(ret).error();
      SET_EXCEPTION(arguments, except);
      return false;
    }
    return true;
  }
};
template <typename T>
struct HandleSetReturn<ExceptionOr<T>> {
  static bool SetReturn(const CallbackArguments& arguments,
                        const ExceptionOr<T>& ret) {
    if (holds_alternative<T>(ret)) {
      SET_RETURN_VALUE(arguments, ToJsValue(get<T>(ret)));
      return true;
    } else {
      LocalVar<JsValue> except = get<js::JsError>(ret).error();
      SET_EXCEPTION(arguments, except);
      return false;
    }
  }
};


/**
 * A helper that calls the given callback then converts and sets the JavaScript
 * return value.
 */
template <typename Ret>
struct CallAndSetReturn {
  template <typename Func, typename... Args>
  static bool Call(const CallbackArguments& arguments, Func callback,
                   Args&&... args) {
    static_assert(!std::is_rvalue_reference<Ret>::value,
                  "Cannot return an rvalue reference.");
    Ret ret = callback(std::forward<Args>(args)...);
    return HandleSetReturn<Ret>::SetReturn(arguments, ret);
  }
};
template <>
struct CallAndSetReturn<void> {
  template <typename Func, typename... Args>
  static bool Call(const CallbackArguments& arguments, Func callback,
                   Args&&... args) {
    callback(std::forward<Args>(args)...);
    return true;
  }
};


// Iterative case.
template <size_t ArgIndex, size_t ArgCount>
struct CallHelper {
  template <typename Func, typename... GivenArgs>
  static bool ConvertAndCallFunction(const std::string& func_name,
                                     const std::string& target_name,
                                     bool is_member_func,
                                     const CallbackArguments& arguments,
                                     Func&& callback, GivenArgs&&... args) {
    using CurType =
        typename util::func_traits<Func>::template argument_type<ArgIndex>;
    return Helper<CurType>::ConvertAndCallFunction(
        func_name, target_name, is_member_func, arguments,
        std::forward<Func>(callback), std::forward<GivenArgs>(args)...);
  }

 private:
  template <typename CurType, typename = void>
  struct Helper {
    template <typename Func, typename... GivenArgs>
    static bool ConvertAndCallFunction(const std::string& func_name,
                                       const std::string& target_name,
                                       bool is_member_func,
                                       const CallbackArguments& arguments,
                                       Func&& callback,
                                       GivenArgs&&... args) {
      using Ret = typename util::func_traits<Func>::return_type;
      RawType<CurType> arg;
      if (ArgumentCount(arguments) + is_member_func <= ArgIndex) {
        if (!is_optional<RawType<CurType>>::value) {
          const size_t arg_count = util::func_traits<Func>::argument_count;
          return ThrowError<std::is_same<Ret, Promise>::value>::NotEnoughArgs(
              &arguments, func_name, target_name, arg_count,
              ArgumentCount(arguments));
        }
      } else {
        // Only convert the argument if it is present.  A missing argument
        // should always be Nothing even if we can convert undefined to a value.
        LocalVar<JsValue> source = is_member_func && ArgIndex == 0
                                       ? GET_ARG_THIS(arguments)
                                       : arguments[ArgIndex - is_member_func];
        if (!FromJsValue(source, &arg)) {
          const std::string type_name = TypeName<RawType<CurType>>::name();
          const std::string value = ConvertToString(source);
          return ThrowError<std::is_same<Ret, Promise>::value>::CannotConvert(
              &arguments, func_name, target_name, value, type_name);
        }
      }

      return CallHelper<ArgIndex + 1, ArgCount>::ConvertAndCallFunction(
          func_name, target_name, is_member_func, arguments,
          std::forward<Func>(callback), std::forward<GivenArgs>(args)...,
          std::move(arg));
    }
  };
  // Special case for CallbackArguments.
  template <typename T>
  struct Helper<const CallbackArguments&, T> {
    static_assert(ArgIndex + 1 == ArgCount,
                  "CallbackArguments must appear last");
    template <typename Func, typename... GivenArgs>
    static bool ConvertAndCallFunction(const std::string& func_name,
                                       const std::string& target_name,
                                       bool is_member_func,
                                       const CallbackArguments& arguments,
                                       Func&& callback,
                                       GivenArgs&&... args) {
      return CallHelper<ArgIndex + 1, ArgCount>::ConvertAndCallFunction(
          func_name, target_name, is_member_func, arguments,
          std::forward<Func>(callback), std::forward<GivenArgs>(args)...,
          arguments);
    }
  };
};

// Base-case.
template <size_t ArgCount>
struct CallHelper<ArgCount, ArgCount> {
  template <typename Func, typename... GivenArgs>
  static bool ConvertAndCallFunction(const std::string& func_name,
                                     const std::string& target_name,
                                     bool is_member_func,
                                     const CallbackArguments& arguments,
                                     Func&& callback, GivenArgs&&... args) {
    using Ret = typename util::func_traits<Func>::return_type;
    return CallAndSetReturn<Ret>::Call(arguments, std::forward<Func>(callback),
                                       std::forward<GivenArgs>(args)...);
  }
};


// Contains extra data passed to registered methods to help call the correct
// backing methods.
struct InternalCallbackDataBase {
  virtual ~InternalCallbackDataBase() {}
};

template <typename Func>
struct InternalCallbackData : InternalCallbackDataBase {
  template <typename T>
  explicit InternalCallbackData(T&& arg) : callback(std::forward<T>(arg)) {}

  typename std::decay<Func>::type callback;
  std::string name;
  std::string target;
  bool is_member_func;
};

constexpr const char* kHiddenPropertyName = "$__shaka_extra_data";

#if defined(USING_JSC)

inline void InternalCallbackDataFinalize(JSObjectRef object) {
  void* ptr = JSObjectGetPrivate(object);
  delete reinterpret_cast<InternalCallbackDataBase*>(ptr);
}

const JSClassDefinition kInternalCallbackDataDefinition = {
    .version = 1,
    .className = "InternalCallbackData",
    .finalize = &InternalCallbackDataFinalize,
};

inline JSClassRef GetCallbackDataClass() {
  static JSClassRef cls = nullptr;
  if (!cls) {
    CHECK(cls = JSClassCreate(&kInternalCallbackDataDefinition));
  }
  return cls;
}

#endif


/**
 * Creates extra data of the given type that will be passed to the callbacks
 * when it is called.  This allows us to pass internal data to specific function
 * objects.
 */
template <typename T, typename Arg>
ReturnVal<JsValue> CreateInternalData(T** extra_data, Arg&& arg) {
#ifdef USING_V8
  v8::Local<v8::ArrayBuffer> ret =
      v8::ArrayBuffer::New(GetIsolate(), sizeof(T));

  void* ptr = ret->GetContents().Data();
  *extra_data = new (ptr) T(std::forward<Arg>(arg));
  // Sanity check: verify the new object pointer is at the start of the data.
  DCHECK_EQ(reinterpret_cast<void*>(*extra_data), ptr);
  JsEngine::Instance()->AddDestructor(ptr, [](void* obj) {
    reinterpret_cast<T*>(obj)->~T();
  });

  return ret;
#elif defined(USING_JSC)
  *extra_data = new T(std::forward<Arg>(arg));

  // IMPORTANT: We need to cast since the argument is a void*.  The finalize
  // method will cast to this type, so the static_cast ensures we pass the
  // correct sub-object pointer.
  LocalVar<JsObject> ret =
      JSObjectMake(GetContext(), GetCallbackDataClass(),
                   static_cast<InternalCallbackDataBase*>(*extra_data));
  CHECK(ret);
  return ret;
#endif
}

/**
 * Pulls internal data |Data| from the given arguments structure.  The internal
 * data must be a POD type because it is stored as a binary blob in an
 * ArrayBuffer.
 *
 * This needs to accept |T| to avoid invalid reinterpret_cast<T> usages.  In
 * V8, we store the type directly in the void*; in JSC, the void*
 * stores the base type.
 */
#ifdef USING_V8
template <typename T, typename Info>
T* GetInternalData(const Info& arguments) {
  if (arguments.Data().IsEmpty() || !arguments.Data()->IsArrayBuffer()) {
    ThrowError</* ReturnPromise */ false>::General(
        nullptr, "", "", "INTERNAL: Invalid function data.");
    return nullptr;
  }
  v8::Local<v8::ArrayBuffer> extra =
      arguments.Data().template As<v8::ArrayBuffer>();
  return reinterpret_cast<T*>(extra->GetContents().Data());
}
#elif defined(USING_JSC)
template <typename T>
T* GetInternalData(const CallbackArguments& arguments) {
  JSValueRef data = GetMemberRaw(arguments.callee(), kHiddenPropertyName);
  void* ptr = IsObject(data) ? JSObjectGetPrivate(UnsafeJsCast<JsObject>(data))
                             : nullptr;
  if (!ptr) {
    ThrowError</* ReturnPromise */ false>::General(
        nullptr, "", "", "INTERNAL: Invalid function data.");
    return nullptr;
  }
  return static_cast<T*>(reinterpret_cast<InternalCallbackDataBase*>(ptr));
}
#endif


template <typename Func>
class JsCallback {
 public:
#if defined(USING_V8)
  static void Call(const CallbackArguments& arguments) {
    v8::HandleScope handle_scope(GetIsolate());
    CallRaw(arguments);
  }
#elif defined(USING_JSC)
  static JSValueRef Call(JSContextRef cx, JSObjectRef callee,
                         JSObjectRef thisv, size_t arg_count,
                         const JSValueRef* args, JSValueRef* except) {
    // TODO(modmaker): Why is this not true?  Should we force use of cx?
    // DCHECK_EQ(cx, GetContext());
    DCHECK(except);
    CallbackArguments arguments(args, arg_count, callee, thisv, except);
    if (!CallRaw(arguments))
      return nullptr;
    return arguments.ret();
  }
#endif

 private:
  static bool CallRaw(const CallbackArguments& arguments) {
    auto data = GetInternalData<InternalCallbackData<Func>>(arguments);
    if (!data)
      return false;

    constexpr const size_t ArgCount = util::func_traits<Func>::argument_count;
    return CallHelper<0, ArgCount>::ConvertAndCallFunction(
        data->name, data->target, data->is_member_func, arguments,
        data->callback);
  }
};


/**
 * A helper type that gets the type of a static Create() method, if it exists.
 * This is used both to determine if a Create() method exists and to unpack the
 * argument types.
 */
template <typename This, typename = void>
struct get_create_type {
  using type = void;
};
template <typename This>
struct get_create_type<This, decltype(void(&This::Create))> {
  using type = decltype(&This::Create);
};

/**
 * This handles either constructing a new C++ object and wrapping it, or
 * throwing an exception if it can't be constructed in JavaScript.  This is
 * handled similar to a normal function WRT to argument conversions.  Then a
 * special case of CallHelper will construct the C++ and JavaScript objects and
 * link them together.
 */
template <typename This, typename Create = typename get_create_type<This>::type>
struct JsConstructorCreateOrThrow;
template <typename This>
struct JsConstructorCreateOrThrow<This, void> {
  static bool CreateOrThrow(const CallbackArguments& arguments) {
    // There is no Create method.
    const std::string type_name = TypeName<This>::name();
    return ThrowError</* ReturnPromise */ false>::IllegalInvocation(
        nullptr, "constructor", type_name);
  }
};
template <typename This, typename... Args>
struct JsConstructorCreateOrThrow<This, This*(*)(Args...)> {
  static bool CreateOrThrow(const CallbackArguments& arguments) {
    // There is a Create method.
    const std::string type_name = TypeName<This>::name();
    auto ctor = [&](Args... args) {
      BackingObject* backing = This::Create(std::forward<Args>(args)...);
      CHECK(ConstructWrapperObject(arguments, backing));
    };
    return CallHelper<0, sizeof...(Args)>::ConvertAndCallFunction(
        "constructor", type_name, false, arguments, std::move(ctor));
  }
};


/**
 * This defines the callback that is invoked when a JavaScript constructor is
 * called.
 *
 * This is slightly different than JsConstructorCreateOrThrow since it needs to
 * handle C++ created objects.  When we create a C++ object and pass to
 * JavaScript, we need to create a JavaScript object for it.  We still need to
 * invoke the same constructor for this case.  This class handles this special
 * case and then forwards to JsConstructorCreateOrThrow.
 */
template <typename This>
class JsConstructor {
 public:
#if defined(USING_V8)
  static void Call(const v8::FunctionCallbackInfo<v8::Value>& arguments) {
    v8::HandleScope handle_scope(GetIsolate());

    const bool is_valid = !arguments.This().IsEmpty() &&
                          arguments.This()->InternalFieldCount() ==
                              BackingObject::kInternalFieldCount;
    CallRaw(arguments, is_valid);
  }
#elif defined(USING_JSC)
  static JSObjectRef Call(JSContextRef cx, JSObjectRef callee, size_t arg_count,
                          const JSValueRef* args, JSValueRef* except) {
    // TODO(modmaker): Why is this not true?  Should we force use of cx?
    // DCHECK_EQ(cx, GetContext());
    DCHECK(except);
    CallbackArguments arguments(args, arg_count, callee, nullptr, except);
    if (!CallRaw(arguments, true))
      return nullptr;
    return UnsafeJsCast<JsObject>(arguments.ret());
  }
#endif

 private:
  static bool CallRaw(const CallbackArguments& arguments, bool is_valid) {
    if (!is_valid) {
      const std::string type_name = TypeName<This>::name();
      return ThrowError</* ReturnPromise */ false>::IllegalInvocation(
          nullptr, "constructor", type_name);
    }

    // Special case to wrap existing object.  Single object of type v8::External
    if (ArgumentCount(arguments) == 1) {
      void* ptr = MaybeUnwrapPointer(arguments[0]);
      if (ptr) {
        auto* backing = reinterpret_cast<BackingObject*>(ptr);
        return ConstructWrapperObject(arguments, backing);
      }
    }

    return JsConstructorCreateOrThrow<This>::CreateOrThrow(arguments);
  }
};


template <typename Func>
ReturnVal<JsFunction> CreateJsFunctionFromCallback(const std::string& target,
                                                   const std::string& name,
                                                   Func&& callback,
                                                   bool is_member_func) {
  impl::InternalCallbackData<Func>* data;
  LocalVar<JsValue> js_value =
      impl::CreateInternalData(&data, std::forward<Func>(callback));
  // data->callback already set.
  data->name = name;
  data->target = target;
  data->is_member_func = is_member_func;

#ifdef USING_V8
  return v8::Function::New(GetIsolate()->GetCurrentContext(),
                           &impl::JsCallback<Func>::Call,
                           js_value, util::func_traits<Func>::argument_count,
                           v8::ConstructorBehavior::kThrow).ToLocalChecked();
#elif defined(USING_JSC)
  JSContextRef cx = GetContext();
  LocalVar<JsObject> ret =
      JSObjectMakeFunctionWithCallback(cx, JsStringFromUtf8(name),
                                       &impl::JsCallback<Func>::Call);

  const int attributes = kJSPropertyAttributeReadOnly |
                         kJSPropertyAttributeDontEnum |
                         kJSPropertyAttributeDontDelete;
  JSObjectSetProperty(cx, ret, JsStringFromUtf8(impl::kHiddenPropertyName),
                      js_value, attributes, nullptr);
  return ret;
#endif
}

}  // namespace impl

/**
 * Creates a new JavaScript function object that will call the given C++
 * callback when it is called.  This will convert the JavaScript arguments into
 * their C++ values and throw JavaScript errors if there are invalid arguments.
 * This callback can throw an instance of JsError to be converted to a
 * JavaScript exception.  The callback will be invoked on the event thread.
 *
 * @param target The name of the |this| object, used for exception messages.
 * @param name The name of the function, used for exception messages.
 * @param callback The callback to invoke.
 * @return The resulting function object.
 */
template <typename Func>
ReturnVal<JsFunction> CreateStaticFunction(const std::string& target,
                                           const std::string& name,
                                           Func&& callback) {
  return impl::CreateJsFunctionFromCallback(
      target, name, std::forward<Func>(callback), false);
}

/**
 * @see CreateStaticFunction
 * The first argument given to the callback will be the |this| argument from
 * JavaScript.  It should be a RefPtr<T> of an appropriate type.
 */
template <typename Func>
ReturnVal<JsFunction> CreateMemberFunction(const std::string& target,
                                           const std::string& name,
                                           Func&& callback) {
  return impl::CreateJsFunctionFromCallback(target, name,
                                            std::forward<Func>(callback), true);
}

/**
 * Registers a static function on the global object.  This is a static function
 * and will ignore |this|.
 *
 * @param name The name of the function (used for errors).
 * @param callback The callback function to call.
 */
template <typename Func>
void RegisterGlobalFunction(const std::string& name, Func&& callback) {
  LocalVar<JsFunction> function =
      CreateStaticFunction("window", name, std::forward<Func>(callback));
  LocalVar<JsValue> value(RawToJsValue(function));
  SetMemberRaw(JsEngine::Instance()->global_handle(), name, value);
}

namespace impl {

// Allow passing callable objects to JavaScript through ToJsValue().
template <typename Func>
struct ConvertHelper<Func, _callable_identifier> {
  template <typename T>
  static ReturnVal<JsValue> ToJsValue(T&& source) {
    auto func = CreateStaticFunction("", "", std::forward<T>(source));
    return RawToJsValue(func);
  }
};

}  // namespace impl

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_REGISTER_MEMBER_H_
