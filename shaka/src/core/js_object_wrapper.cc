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

#include "src/core/js_object_wrapper.h"

#include "src/mapping/register_member.h"

namespace shaka {

JsObjectWrapper::JsObjectWrapper() {}
JsObjectWrapper::~JsObjectWrapper() {}

void JsObjectWrapper::Init(Handle<JsObject> object) {
  object_ = object;
}

Error JsObjectWrapper::ConvertError(Handle<JsValue> except) {
  if (!IsObject(except))
    return Error(ConvertToString(except));

  LocalVar<JsObject> obj = UnsafeJsCast<JsObject>(except);
  LocalVar<JsValue> message_member = GetMemberRaw(obj, "message");
  // Rather than checking for the type of the exception, assume that if it has
  // a 'name' field, then it is an exception.
  LocalVar<JsValue> name_member = GetMemberRaw(obj, "name");
  if (!IsNullOrUndefined(name_member)) {
    const std::string message =
        ConvertToString(name_member) + ": " + ConvertToString(message_member);
    return Error(message);
  }

  LocalVar<JsValue> code = GetMemberRaw(obj, "code");
  LocalVar<JsValue> category = GetMemberRaw(obj, "category");
  if (GetValueType(code) != proto::ValueType::Number ||
      GetValueType(category) != proto::ValueType::Number) {
    return Error(ConvertToString(except));
  }

  LocalVar<JsValue> severity_val = GetMemberRaw(obj, "severity");
  int severity = 0;
  if (GetValueType(severity_val) == proto::ValueType::Number)
    severity = static_cast<int>(NumberFromValue(severity_val));

  std::string message;
  if (IsNullOrUndefined(message_member)) {
    message = "Shaka Error, Category: " + ConvertToString(category) +
              ", Code: " + ConvertToString(code);
  } else {
    message = ConvertToString(message_member);
  }
  return Error(severity, static_cast<int>(NumberFromValue(category)),
               static_cast<int>(NumberFromValue(code)), message);
}

JsObjectWrapper::Converter<void>::variant_type
JsObjectWrapper::AttachEventListener(
    const std::string& name, std::function<void(const Error&)> on_error,
    std::function<void(Handle<JsObject> event)> handler) const {
  const std::function<void(optional<Any>)> callback = [=](optional<Any> event) {
    // We can't accept or use events::Event since Shaka player raises fake
    // events.  So manually look for the properties.
    LocalVar<JsValue> event_val = ToJsValue(event);
    if (!IsObject(event_val)) {
      on_error(Error(ConvertToString(event_val)));
      return;
    }

    LocalVar<JsObject> event_obj = UnsafeJsCast<JsObject>(event_val);
    handler(event_obj);
  };
  LocalVar<JsFunction> callback_js = CreateStaticFunction("", "", callback);
  LocalVar<JsValue> arguments[] = {ToJsValue(name), RawToJsValue(callback_js)};
  return CallMemberFunction(object_, "addEventListener", 2, arguments, nullptr);
}

JsObjectWrapper::Converter<void>::variant_type
JsObjectWrapper::CallMemberFunction(Handle<JsObject> that,
                                    const std::string& name, int argc,
                                    LocalVar<JsValue>* argv,
                                    LocalVar<JsValue>* result) {
  LocalVar<JsValue> member = GetMemberRaw(that, name);
  if (GetValueType(member) != proto::ValueType::Function) {
    return Error("The member '" + name + "' is not a function.");
  }

  LocalVar<JsValue> result_or_except;
  LocalVar<JsFunction> member_func = UnsafeJsCast<JsFunction>(member);
  if (!InvokeMethod(member_func, that, argc, argv, &result_or_except)) {
    return ConvertError(result_or_except);
  }

  if (result)
    *result = result_or_except;
  return monostate();
}

}  // namespace shaka
