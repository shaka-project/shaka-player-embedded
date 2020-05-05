// Copyright 2019 Google LLC
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

#ifndef SHAKA_EMBEDDED_CORE_JS_OBJECT_WRAPPER_H_
#define SHAKA_EMBEDDED_CORE_JS_OBJECT_WRAPPER_H_

#include <future>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "shaka/async_results.h"
#include "shaka/error.h"
#include "shaka/variant.h"
#include "src/core/js_manager_impl.h"
#include "src/core/task_runner.h"
#include "src/mapping/convert_js.h"
#include "src/mapping/js_engine.h"
#include "src/mapping/js_utils.h"
#include "src/mapping/js_wrappers.h"
#include "src/mapping/promise.h"
#include "src/util/macros.h"

namespace shaka {

/**
 * A helper that wraps a JavaScript-defined object.  This is meant to be used
 * by the public types.  This means this object holds a strong reference to the
 * given JavaScript object and this returns the public types instead of using
 * internal types (e.g. JsError).
 */
class JsObjectWrapper {
 public:
  JsObjectWrapper();
  ~JsObjectWrapper();

  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(JsObjectWrapper);

  /**
   * A helper that converts a JavaScript value into the given return type as a
   * variant.  If the return type is |void|, this ignores the value.
   */
  template <typename Ret, typename = void>
  struct Converter {
    using variant_type = typename AsyncResults<Ret>::variant_type;
    using future_type = std::shared_future<variant_type>;

    static variant_type Convert(const std::string& name,
                                Handle<JsValue> result) {
      Ret ret;
      if (!FromJsValue(result, &ret)) {
        return Error("Invalid return value from " + name + "().");
      }

      return ret;
    }
  };
  template <typename Dummy>
  struct Converter<void, Dummy> {
    using variant_type = AsyncResults<void>::variant_type;
    using future_type = std::shared_future<variant_type>;

    static variant_type Convert(const std::string& /* name */,
                                Handle<JsValue> /* result */) {
      return monostate();
    }
  };

  /**
   * Calls the given member method and converts the returned value to the given
   * type.  If the function returns a Promise, this waits for the Promise to
   * resolve first.
   *
   * @param name The name of the member method to call.
   * @param args The arguments to pass to the function.
   * @return A Future that will contain the converted return value.
   */
  template <typename Ret, typename... Args>
  typename Converter<Ret>::future_type CallMethod(const std::string& name,
                                                  Args&&... args) const {
    return CallMethodCommon<Ret>(&object_, name, std::forward<Args>(args)...);
  }

  /**
   * Calls the given global method and converts the returned value to the given
   * type.  If the function returns a Promise, this waits for the Promise to
   * resolve first.
   *
   * @param global_path The "path" to get to the method.
   * @param args The arguments to pass to the function.
   * @return A Future that will contain the converted return value.
   */
  template <typename Ret, typename... Args>
  static typename Converter<Ret>::future_type CallGlobalMethod(
      const std::vector<std::string>& global_path, Args&&... args) {
    std::vector<std::string> obj_path{global_path.begin(),
                                      global_path.end() - 1};
    return CallMethodCommon<Ret>(std::move(obj_path), global_path.back(),
                                 std::forward<Args>(args)...);
  }

  /**
   * Gets a field of a global object and returns the converted value.
   *
   * @param global_path The "path" to get to the field.
   * @return A Future that will contain the converted field value.
   */
  template <typename T>
  static typename Converter<T>::future_type GetGlobalField(
      const std::vector<std::string>& global_path) {
    auto callback = std::bind(&GetFieldRaw<T>, global_path);
    return JsManagerImpl::Instance()->MainThread()->InvokeOrSchedule(
        std::move(callback));
  }

  /**
   * Converts the given JavaScript error object into a public Error object.
   */
  static Error ConvertError(Handle<JsValue> except);

  /** Sets the object instance to use. */
  void Init(Handle<JsObject> object);

 protected:
  /**
   * Calls the given member of the given object.  If this throws an error, this
   * returns it; otherwise it stores the return value in |*result|.  This can
   * only be called on the JS main thread.
   */
  static Converter<void>::variant_type CallMemberFunction(
      Handle<JsObject> that, const std::string& name, int argc,
      LocalVar<JsValue>* argv, LocalVar<JsValue>* result);

  /**
   * Attaches an event listener so the given callback is called when the given
   * event is called.
   *
   * @param name The name of the event to listen for.
   * @param on_error A callback that is called if there is an error converting
   *   the arguments in the listener.
   * @param handler The callback that is called when the event is raised.
   */
  Converter<void>::variant_type AttachEventListener(
      const std::string& name, std::function<void(const Error&)> on_error,
      std::function<void(Handle<JsObject> event)> handler) const;

  /**
   * Gets a field's value and converts it to the given type.  This can only be
   * called on the JS main thread.
   *
   * @param global_path The "path" to get to the field.
   * @return The converted value.
   */
  template <typename T>
  static typename Converter<T>::variant_type GetFieldRaw(
      const std::vector<std::string>& global_path) {
    LocalVar<JsValue> value =
        GetDescendant(JsEngine::Instance()->global_handle(), global_path);
    return Converter<T>::Convert(global_path.back(), value);
  }

  /**
   * Calls the given function and stores the result in the given Promise.  This
   * can only be called on the JS main thread.
   *
   * @param p The Promise that will be given the results
   * @param that The "this" object; either an instance object or a "path" for
   *   a global object.
   * @param name The name of the member to call.
   * @param args The arguments to pass to the call.
   */
  template <typename Ret, typename... Args>
  static void CallMethodRaw(
      std::shared_ptr<std::promise<typename Converter<Ret>::variant_type>> p,
      variant<const Global<JsObject>*, std::vector<std::string>> that,
      const std::string& name, Args&&... args) {
    DCHECK(JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
    LocalVar<JsObject> that_obj;
    if (holds_alternative<const Global<JsObject>*>(that)) {
      that_obj = *get<const Global<JsObject>*>(that);
    } else {
      LocalVar<JsValue> temp =
          GetDescendant(JsEngine::Instance()->global_handle(),
                        get<std::vector<std::string>>(that));
      if (!IsObject(temp)) {
        p->set_value(Error("Unable to find object."));
        return;
      }
      that_obj = UnsafeJsCast<JsObject>(temp);
    }

    LocalVar<JsValue> result;
    LocalVar<JsValue> js_args[] = {ToJsValue(args)..., JsUndefined()};
    auto error =
        CallMemberFunction(that_obj, name, sizeof...(args), js_args, &result);
    if (holds_alternative<Error>(error)) {
      p->set_value(get<Error>(error));
      return;
    }

    auto js_promise = Converter<Promise>::Convert(name, result);
    if (holds_alternative<Error>(js_promise)) {
      p->set_value(Converter<Ret>::Convert(name, result));
      return;
    }

    get<Promise>(js_promise)
        .Then(
            [p, name](Any res) {
              LocalVar<JsValue> value = res.ToJsValue();
              p->set_value(Converter<Ret>::Convert(name, value));
            },
            [p](Any except) {
              LocalVar<JsValue> val = except.ToJsValue();
              p->set_value(ConvertError(val));
            });
  }

  /**
   * When using std::bind, the arguments are stored in fields within the
   * returned object.  But we use perfect forwarding to accept arguments, so we
   * may accept an lvalue or an rvalue.  This means we need to convert the type
   * we get to a const-lvalue so the field can be passed to the function.
   */
  template <typename T>
  using bind_forward =
      typename std::add_const<typename std::remove_reference<T>::type>::type&;

  /**
   * Calls the given function, returning a Promise to the value.  If this is
   * called form the main thread, invokes it immediately; otherwise schedules it
   * to be called later.
   *
   * @param that The "this" object; either an instance object or a "path" for
   *   a global object.
   * @param name The name of the member to call.
   * @param args The arguments to pass to the call.
   * @return A Future to the converted return value of the call.
   */
  template <typename Ret, typename... Args>
  static typename Converter<Ret>::future_type CallMethodCommon(
      variant<const Global<JsObject>*, std::vector<std::string>> that,
      const std::string& name, Args&&... args) {
    auto promise =
        std::make_shared<std::promise<typename Converter<Ret>::variant_type>>();
    if (JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread()) {
      CallMethodRaw<Ret>(promise, std::move(that), name,
                         std::forward<Args>(args)...);
    } else {
      auto callback =
          std::bind(&CallMethodRaw<Ret, bind_forward<Args>...>, promise,
                    std::move(that), name, std::forward<Args>(args)...);
      JsManagerImpl::Instance()->MainThread()->AddInternalTask(
          TaskPriority::Internal, name, std::move(callback));
    }
    return promise->get_future().share();
  }

  Global<JsObject> object_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_CORE_JS_OBJECT_WRAPPER_H_
