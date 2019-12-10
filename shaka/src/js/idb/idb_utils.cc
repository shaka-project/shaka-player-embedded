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

#include "src/js/idb/idb_utils.h"

#include <glog/logging.h>

#include <vector>

#include "src/js/js_error.h"
#include "src/mapping/byte_buffer.h"
#include "src/mapping/convert_js.h"
#include "src/mapping/js_wrappers.h"
#include "src/util/utils.h"

namespace shaka {
namespace js {
namespace idb {

namespace {

ReturnVal<JsValue> ToJsObject(bool value) {
#ifdef USING_V8
  return v8::BooleanObject::New(GetIsolate(), value);
#elif defined(USING_JSC)
  LocalVar<JsValue> value_js = ToJsValue(value);
  JSValueRef arg = value_js;
  return CreateNativeObject("Boolean", &arg, 1);
#endif
}

ReturnVal<JsValue> ToJsObject(double value) {
#ifdef USING_V8
  return v8::NumberObject::New(GetIsolate(), value);
#elif defined(USING_JSC)
  LocalVar<JsValue> value_js = ToJsValue(value);
  JSValueRef arg = value_js;
  return CreateNativeObject("Number", &arg, 1);
#endif
}

ReturnVal<JsValue> ToJsObject(const std::string& value) {
#ifdef USING_V8
  return v8::StringObject::New(GetIsolate(), JsStringFromUtf8(value));
#elif defined(USING_JSC)
  LocalVar<JsValue> value_js = ToJsValue(value);
  JSValueRef arg = value_js;
  return CreateNativeObject("String", &arg, 1);
#endif
}


ExceptionOr<void> StoreValue(Handle<JsValue> input, proto::Value* output,
                             std::vector<ReturnVal<JsValue>>* memory);
ReturnVal<JsValue> InternalFromStored(const proto::Value& item);

ExceptionOr<void> StoreObject(proto::ValueType kind, Handle<JsObject> object,
                              proto::Object* output,
                              std::vector<Handle<JsValue>>* memory) {
  if (kind == proto::Array)
    output->set_array_length(ArrayLength(object));
  else
    output->clear_array_length();

  output->clear_entries();
  for (const std::string& property : GetMemberNames(object)) {
    proto::Object::Entry* child = output->add_entries();

    // Call [[Get]], rethrowing any exception thrown.
    LocalVar<JsValue> except;
    LocalVar<JsValue> value = GetMemberRaw(object, property, &except);
    if (!IsNullOrUndefined(except))
      return JsError::Rethrow(except);

    child->set_key(property);
    RETURN_IF_ERROR(StoreValue(value, child->mutable_value(), memory));
  }
  return {};
}

ExceptionOr<void> StoreValue(Handle<JsValue> input, proto::Value* output,
                             std::vector<ReturnVal<JsValue>>* memory) {
  const proto::ValueType kind = GetValueType(input);

  // Store objects we have seen and throw error if we see duplicates.
  if (IsObject(input)) {
    if (util::contains(*memory, input)) {
      return JsError::DOMException(
          DataCloneError,
          "Duplicate copies of the same object are not supported.");
    }
    memory->push_back(input);
  }

  output->Clear();
  output->set_kind(kind);
  switch (kind) {
    case proto::Undefined:
    case proto::Null:
      break;
    case proto::Boolean:
    case proto::BooleanObject:
      output->set_value_bool(BooleanFromValue(input));
      break;
    case proto::Number:
    case proto::NumberObject:
      output->set_value_number(NumberFromValue(input));
      break;
    case proto::String:
    case proto::StringObject: {
      std::string str = ConvertToString(input);
      output->set_value_string(str.data(), str.size());
      break;
    }

    case proto::ArrayBuffer:
    case proto::Int8Array:
    case proto::Uint8Array:
    case proto::Uint8ClampedArray:
    case proto::Int16Array:
    case proto::Uint16Array:
    case proto::Int32Array:
    case proto::Uint32Array:
    case proto::Float32Array:
    case proto::Float64Array:
    case proto::DataView: {
      ByteBuffer buffer;
      CHECK(buffer.TryConvert(input));
      output->set_value_bytes(buffer.data(), buffer.size());
      break;
    }

    case proto::Array:
    case proto::OtherObject: {
      // This must be either an anonymous object, an array, or a wrapper object.
      LocalVar<JsObject> object = UnsafeJsCast<JsObject>(input);
      if (kind != proto::Array && IsBuiltInObject(object)) {
        return JsError::DOMException(DataCloneError);
      }

      // Arrays and objects are treated the same.
      proto::Object* output_object = output->mutable_value_object();
      return StoreObject(kind, object, output_object, memory);
    }
    case proto::Function:
    default:
      return JsError::DOMException(DataCloneError);
  }

  return {};
}


ReturnVal<JsValue> FromStoredObject(const proto::Object& object) {
  LocalVar<JsObject> ret;
  if (object.has_array_length())
    ret = CreateArray(object.array_length());
  else
    ret = CreateObject();

  for (const proto::Object::Entry& entry : object.entries()) {
    LocalVar<JsValue> value = InternalFromStored(entry.value());
    SetMemberRaw(ret, entry.key(), value);
  }

  return RawToJsValue(ret);
}

ReturnVal<JsValue> InternalFromStored(const proto::Value& item) {
  DCHECK(item.IsInitialized());
  switch (item.kind()) {
    case proto::Undefined:
      return JsUndefined();
    case proto::Null:
      return JsNull();
    case proto::Boolean:
      DCHECK(item.has_value_bool());
      return ToJsValue(item.value_bool());
    case proto::Number:
      DCHECK(item.has_value_number());
      return ToJsValue(item.value_number());
    case proto::String:
      DCHECK(item.has_value_string());
      return ToJsValue(item.value_string());

    case proto::BooleanObject:
      DCHECK(item.has_value_bool());
      return ToJsObject(item.value_bool());
    case proto::NumberObject:
      DCHECK(item.has_value_number());
      return ToJsObject(item.value_number());
    case proto::StringObject:
      DCHECK(item.has_value_string());
      return ToJsObject(item.value_string());

    case proto::ArrayBuffer:
    case proto::Int8Array:
    case proto::Uint8Array:
    case proto::Uint8ClampedArray:
    case proto::Int16Array:
    case proto::Uint16Array:
    case proto::Int32Array:
    case proto::Uint32Array:
    case proto::Float32Array:
    case proto::Float64Array:
    case proto::DataView: {
      DCHECK(item.has_value_bytes());
      const std::string& str = item.value_bytes();
      ByteBuffer temp(reinterpret_cast<const uint8_t*>(&str[0]), str.size());
      return temp.ToJsValue(item.kind());
    }
    case proto::Array:
    case proto::OtherObject:
      return FromStoredObject(item.value_object());
    default:
      LOG(FATAL) << "Invalid stored value " << item.kind();
  }
}

}  // namespace

ExceptionOr<void> StoreInProto(Any input, proto::Value* result) {
  std::vector<ReturnVal<JsValue>> seen;
  return StoreValue(input.ToJsValue(), result, &seen);
}

Any LoadFromProto(const proto::Value& value) {
  Any ret;
  CHECK(ret.TryConvert(InternalFromStored(value)));
  return ret;
}

}  // namespace idb
}  // namespace js
}  // namespace shaka
