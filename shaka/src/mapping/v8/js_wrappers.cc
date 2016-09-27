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

#include "src/mapping/js_wrappers.h"

#include "src/mapping/backing_object.h"
#include "src/mapping/convert_js.h"
#include "src/util/file_system.h"

namespace shaka {

namespace {

class StaticExternalResource
    : public v8::String::ExternalOneByteStringResource {
 public:
  StaticExternalResource(const uint8_t* data, size_t data_size)
      : data_(data), data_size_(data_size) {}

  NON_COPYABLE_OR_MOVABLE_TYPE(StaticExternalResource);

  const char* data() const override {
    return reinterpret_cast<const char*>(data_);
  }

  size_t length() const override {
    return data_size_;
  }

 protected:
  void Dispose() override {
    delete this;
  }

 private:
  ~StaticExternalResource() override {}

  const uint8_t* data_;
  size_t data_size_;
};

v8::Local<v8::String> MakeExternalString(const uint8_t* data,
                                         size_t data_size) {
#ifndef NDEBUG
  for (size_t i = 0; i < data_size; i++)
    CHECK_LT(data[i], 0x80) << "External string must be ASCII";
#endif

  auto* res = new StaticExternalResource(data, data_size);
  auto temp = v8::String::NewExternalOneByte(GetIsolate(), res);  // NOLINT
  return temp.ToLocalChecked();
}

template <typename T>
ReturnVal<JsValue> GetMemberImpl(Handle<JsObject> object, T ind) {
  v8::Isolate* isolate = GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::MaybeLocal<v8::Value> maybe_field = object->Get(context, ind);
  v8::Local<v8::Value> field;
  if (!maybe_field.ToLocal(&field))
    return v8::Undefined(isolate);
  return field;
}

template <typename T>
void SetMemberImpl(Handle<JsObject> object, T ind, Handle<JsValue> value) {
  auto context = GetIsolate()->GetCurrentContext();
  // Set returns Maybe<bool>, which is nothing on error.
  CHECK(object->Set(context, ind, value).IsJust());
}

bool RunScriptImpl(const std::string& path, Handle<JsString> source) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  auto context = isolate->GetCurrentContext();
  v8::HandleScope handle_scope(GetIsolate());
  v8::ScriptOrigin origin(ToJsValue(path));

  // Compile the script.
  v8::TryCatch trycatch(isolate);
  v8::MaybeLocal<v8::Script> maybe_script =
      v8::Script::Compile(context, source, &origin);
  v8::Local<v8::Script> script;
  if (!maybe_script.ToLocal(&script) || script.IsEmpty()) {
    LOG(ERROR) << "Error loading script " << path;
    return false;
  }

  // Run the script.  Run() returns the return value from the script, which
  // will be empty if the script failed to execute.
  if (script->Run(context).IsEmpty()) {
    OnUncaughtException(trycatch.Exception(), false);
    return false;
  }
  return true;
}

}  // namespace

std::vector<std::string> GetMemberNames(Handle<JsObject> object) {
  std::vector<std::string> names;

  v8::Isolate* isolate = GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::MaybeLocal<v8::Array> maybe_field = object->GetOwnPropertyNames(context);
  v8::Local<v8::Array> array;
  if (maybe_field.ToLocal(&array)) {
    for (size_t i = 0; i < array->Length(); i++) {
      v8::Local<v8::Value> name = GetArrayIndexRaw(array, i);
      names.push_back(ConvertToString(name));
    }
  }

  return names;
}

ReturnVal<JsValue> GetMemberRaw(Handle<JsObject> object,
                                const std::string& name) {
  return GetMemberImpl(object, JsStringFromUtf8(name));
}

ReturnVal<JsValue> GetArrayIndexRaw(Handle<JsObject> object, size_t index) {
  return GetMemberImpl(object, index);
}

void SetMemberRaw(Handle<JsObject> object, const std::string& name,
                  Handle<JsValue> value) {
  SetMemberImpl(object, JsStringFromUtf8(name), value);
}

void SetArrayIndexRaw(Handle<JsObject> object, size_t i,
                      Handle<JsValue> value) {
  SetMemberImpl(object, i, value);
}


void SetGenericPropertyRaw(Handle<JsObject> object, const std::string& name,
                           Handle<JsFunction> getter,
                           Handle<JsFunction> setter) {
  object->SetAccessorProperty(JsStringFromUtf8(name), getter, setter);
}


bool InvokeConstructor(Handle<JsFunction> ctor, int argc,
                       LocalVar<JsValue>* argv,
                       LocalVar<JsValue>* result_or_except) {
  v8::Isolate* isolate = GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::EscapableHandleScope handles(isolate);

  v8::TryCatch trycatch(isolate);
  v8::MaybeLocal<v8::Object> result = ctor->NewInstance(context, argc, argv);
  if (result.IsEmpty()) {
    *result_or_except = handles.Escape(trycatch.Exception());
    return false;
  }
  *result_or_except = handles.Escape(result.ToLocalChecked());
  return true;
}

bool InvokeMethod(Handle<JsFunction> method, Handle<JsObject> that, int argc,
                  LocalVar<JsValue>* argv,
                  LocalVar<JsValue>* result_or_except) {
  v8::Isolate* isolate = GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::EscapableHandleScope handles(isolate);

  if (that.IsEmpty())
    that = context->Global();

  v8::TryCatch trycatch(isolate);
  v8::MaybeLocal<v8::Value> result = method->Call(context, that, argc, argv);
  if (result.IsEmpty()) {
    *result_or_except = handles.Escape(trycatch.Exception());
    return false;
  }
  *result_or_except = handles.Escape(result.ToLocalChecked());
  return true;
}


std::string ConvertToString(Handle<JsValue> value) {
  if (!value.IsEmpty() && value->IsSymbol()) {
    LocalVar<v8::Value> name = value.As<v8::Symbol>()->Name();
    return name.IsEmpty() || name->IsUndefined() ? "" : ConvertToString(name);
  }

  v8::String::Utf8Value str(value);
  return std::string(*str, str.length());
}

ReturnVal<JsValue> WrapPointer(void* ptr) {
  return v8::External::New(GetIsolate(), ptr);
}

void* MaybeUnwrapPointer(Handle<JsValue> value) {
  if (value.IsEmpty() || !value->IsExternal())
    return nullptr;
  return value.As<v8::External>()->Value();
}

BackingObject* GetInternalPointer(Handle<JsValue> value) {
  if (value.IsEmpty() || !value->IsObject())
    return nullptr;

  v8::Local<v8::Object> object = value.As<v8::Object>();
  if (object->InternalFieldCount() != BackingObject::kInternalFieldCount)
    return nullptr;

  return reinterpret_cast<BackingObject*>(
      object->GetAlignedPointerFromInternalField(0));
}

bool IsDerivedFrom(BackingObject* ptr, const std::string& name) {
  return ptr ? ptr->DerivedFrom(name) : false;
}

bool RunScript(const std::string& path) {
  util::FileSystem fs;
  std::vector<uint8_t> source;
  CHECK(fs.ReadFile(path, &source));
  v8::Local<v8::String> code =
      v8::String::NewFromUtf8(GetIsolate(),
                              reinterpret_cast<const char*>(source.data()),
                              v8::NewStringType::kNormal, source.size())
          .ToLocalChecked();
  return RunScriptImpl(path, code);
}

bool RunScript(const std::string& path, const uint8_t* data, size_t data_size) {
  v8::Local<v8::String> source = MakeExternalString(data, data_size);
  return RunScriptImpl(path, source);
}

ReturnVal<JsValue> ParseJsonString(const std::string& json) {
  v8::Local<v8::String> source = MakeExternalString(
      reinterpret_cast<const uint8_t*>(json.data()), json.size());
  v8::MaybeLocal<v8::Value> value =
      v8::JSON::Parse(GetIsolate()->GetCurrentContext(), source);
  if (value.IsEmpty())
    return {};
  return value.ToLocalChecked();
}

ReturnVal<JsString> JsStringFromUtf8(const std::string& str) {
  // NewStringType determines where to put the string.
  // - kNormal is for "normal", short-lived strings.
  // - kInternalized is for common strings and are cached (taking up more space)
  // TODO: Investigate using kInternalized for property names which are static
  // and may be common, Chromium has a v8AtomicString method for this.
  return v8::String::NewFromUtf8(GetIsolate(), str.c_str(),
                                 v8::NewStringType::kNormal, str.size())
      .ToLocalChecked();
}

ReturnVal<JsValue> JsUndefined() {
  return v8::Undefined(GetIsolate());
}

ReturnVal<JsValue> JsNull() {
  return v8::Null(GetIsolate());
}

ReturnVal<JsObject> CreateArray(size_t length) {
  return v8::Array::New(GetIsolate(), length);
}

ReturnVal<JsObject> CreateObject() {
  return v8::Object::New(GetIsolate());
}

ReturnVal<JsMap> CreateMap() {
  return v8::Map::New(GetIsolate());
}

void SetMapValue(Handle<JsMap> map, Handle<JsValue> key,
                 Handle<JsValue> value) {
  LocalVar<v8::Context> ctx = GetIsolate()->GetCurrentContext();
  CHECK(!map->Set(ctx, key, value).IsEmpty());
}


bool IsNullOrUndefined(Handle<JsValue> value) {
  return value.IsEmpty() || value->IsNull() || value->IsUndefined();
}

bool IsObject(Handle<JsValue> value) {
  return !value.IsEmpty() && value->IsObject();
}

bool IsBuiltInObject(Handle<JsObject> object) {
  // This calls Object.prototype.toString, which will produce something like
  // '[object Promise]' for built-in types.
  v8::MaybeLocal<v8::String> to_string =
      object->ObjectProtoToString(GetIsolate()->GetCurrentContext());
  if (to_string.IsEmpty())
    return false;
  return ConvertToString(to_string.ToLocalChecked()) != "[object Object]";
}

proto::ValueType GetValueType(Handle<JsValue> value) {
  if (value.IsEmpty())
    return proto::ValueType::Unknown;
  if (value->IsUndefined())
    return proto::ValueType::Undefined;
  if (value->IsNull())
    return proto::ValueType::Null;
  if (value->IsBoolean())
    return proto::ValueType::Boolean;
  if (value->IsNumber())
    return proto::ValueType::Number;
  if (value->IsString())
    return proto::ValueType::String;
  if (value->IsSymbol())
    return proto::ValueType::Symbol;
  if (value->IsFunction())
    return proto::ValueType::Function;
  if (value->IsArray())
    return proto::ValueType::Array;
  if (value->IsPromise())
    return proto::ValueType::Promise;
  if (value->IsBooleanObject())
    return proto::ValueType::BooleanObject;
  if (value->IsNumberObject())
    return proto::ValueType::NumberObject;
  if (value->IsStringObject())
    return proto::ValueType::StringObject;
  if (value->IsArrayBuffer())
    return proto::ValueType::ArrayBuffer;
  if (value->IsInt8Array())
    return proto::ValueType::Int8Array;
  if (value->IsUint8Array())
    return proto::ValueType::Uint8Array;
  if (value->IsUint8ClampedArray())
    return proto::ValueType::Uint8ClampedArray;
  if (value->IsInt16Array())
    return proto::ValueType::Int16Array;
  if (value->IsUint16Array())
    return proto::ValueType::Uint16Array;
  if (value->IsInt32Array())
    return proto::ValueType::Int32Array;
  if (value->IsUint32Array())
    return proto::ValueType::Uint32Array;
  if (value->IsFloat32Array())
    return proto::ValueType::Float32Array;
  if (value->IsFloat64Array())
    return proto::ValueType::Float64Array;
  if (value->IsDataView())
    return proto::ValueType::DataView;
  if (value->IsObject())
    return proto::ValueType::OtherObject;

  // The only thing a value can be is a primitive or an object.  We should
  // be checking every primitive above so it should be an object.
  LOG(WARNING) << "Unknown JavaScript value given=" << ConvertToString(value);
  return proto::ValueType::Unknown;
}

double NumberFromValue(Handle<JsValue> value) {
  DCHECK(!value.IsEmpty());
  if (value->IsNumber()) {
    return value.As<v8::Number>()->Value();
  }
  DCHECK(value->IsNumberObject());
  return value.As<v8::NumberObject>()->ValueOf();
}

bool BooleanFromValue(Handle<JsValue> value) {
  DCHECK(!value.IsEmpty());
  if (value->IsBoolean()) {
    return value->IsTrue();
  }
  DCHECK(value->IsBooleanObject());
  return value.As<v8::BooleanObject>()->ValueOf();
}

}  // namespace shaka
