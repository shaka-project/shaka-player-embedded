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

// Allow struct fields so we can test the GC.  This should only be used in this
// file since struct fields should not be used normally.  This must appear
// before the headers so this is defined when register_member.h is included.
#define ALLOW_STRUCT_FIELDS

#include "src/js/test_type.h"

#include <utility>

#include "src/core/js_manager_impl.h"
#include "src/js/console.h"
#include "src/mapping/js_utils.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {

namespace {

std::string GetExpectedString() {
  // Since this contains embedded nulls, we can't use the normal char*
  // constructor. Instead use the size of the array to get the length of the
  // string literal and subtract one for the ending '\0'.
  return std::string(EXPECTED_STRING,
                     EXPECTED_STRING + sizeof(EXPECTED_STRING) - 1);
}

}  // namespace

DEFINE_STRUCT_SPECIAL_METHODS_COPYABLE(TestTypeOptions);

TestType::TestType() : int_or_object(0) {}
// \cond Doxygen_Skip
TestType::~TestType() {}
// \endcond Doxygen_Skip

void TestType::Trace(memory::HeapTracer* tracer) const {
  BackingObject::Trace(tracer);
  tracer->Trace(&optional_object);
  tracer->Trace(&int_or_object);
  tracer->Trace(&struct_);
  tracer->Trace(&array);
  tracer->Trace(&callback);
  tracer->Trace(&any);
  tracer->Trace(&buffer);
}

bool TestType::IsExpectedString(const std::string& arg) const {
  return arg == GetExpectedString();
}

bool TestType::IsOptionalPresent(optional<std::string> arg) const {
  return arg.has_value();
}

bool TestType::IsExpectedIntWithOr(variant<int, Any> arg) const {
  return holds_alternative<int>(arg) && get<int>(arg) == EXPECTED_INT;
}

bool TestType::IsExpectedStructWithOr(variant<int, TestTypeOptions> arg) const {
  return holds_alternative<TestTypeOptions>(arg) &&
         IsExpectedConvertedStruct(get<TestTypeOptions>(arg));
}

bool TestType::IsExpectedConvertedStruct(TestTypeOptions opts) const {
  return opts.string == GetExpectedString() && opts.boolean;
}

bool TestType::IsConvertedStructEmpty(TestTypeOptions opts) const {
  return opts.string.empty() && !opts.boolean;
}

bool TestType::IsExpectedNumberEnum(TestNumberEnum e) const {
  return e == EXPECTED_NUMBER_ENUM;
}

bool TestType::IsExpectedStringEnum(TestStringEnum e) const {
  return e == EXPECTED_STRING_ENUM;
}

bool TestType::IsExpectedArrayOfStrings(
    const std::vector<std::string>& data) const {
  return data == GetArrayOfStrings();
}

bool TestType::IsExpectedStringWithAny(Any anything) const {
  std::string str;
  return anything.TryConvertTo(&str) && str == GetExpectedString();
}

bool TestType::IsTruthy(Any anything) const {
  return anything.IsTruthy();
}

void TestType::InvokeCallbackWithString(Callback callback) const {
  Callback call_copy =  // NOLINT(performance-unnecessary-copy-initialization)
      callback;
  call_copy(GetExpectedString());
}

void TestType::StoreByteBuffer(ByteBuffer buffer) {
  this->buffer = std::move(buffer);
}

TestTypeOptions TestType::ChangeStringField(TestTypeOptions opts) {
  opts.string = "abc";
  return opts;
}

ExceptionOr<void> TestType::ThrowException(const std::string& message) const {
  return JsError::Error(message);
}

Promise TestType::PromiseAcceptString(const std::string& /* value */) const {
  LocalVar<JsValue> value(JsUndefined());
  return Promise::Resolved(value);
}

Promise TestType::PromiseResolveWith(Any value) const {
  LocalVar<JsValue> rooted(value.ToJsValue());
  return Promise::Resolved(rooted);
}

Promise TestType::PromiseResolveAfter(uint64_t delay) const {
  RefPtr<Promise> ret = MakeJsRef<Promise>(Promise::PendingPromise());
  JsManagerImpl::Instance()->MainThread()->AddTimer(delay, [=]() {
    LocalVar<JsValue> value(JsUndefined());
    ret->ResolveWith(value);
  });
  return *ret;
}

std::string TestType::GetString() const {
  return GetExpectedString();
}

optional<std::string> TestType::GetOptionalString(bool has_value) const {
  if (!has_value)
    return nullopt;
  return GetExpectedString();
}

variant<int, std::string> TestType::GetIntOrString(bool get_int) const {
  if (get_int)
    return EXPECTED_INT;
  return GetExpectedString();
}

TestTypeOptions TestType::GetStruct() const {
  TestTypeOptions ret;
  ret.string = GetExpectedString();
  ret.boolean = true;
  return ret;
}

TestNumberEnum TestType::GetNumberEnum() const {
  return EXPECTED_NUMBER_ENUM;
}

TestStringEnum TestType::GetStringEnum() const {
  return EXPECTED_STRING_ENUM;
}

std::vector<std::string> TestType::GetArrayOfStrings() const {
  return {"abc", "123", GetExpectedString()};
}

std::unordered_map<std::string, std::string> TestType::GetMapOfStrings() const {
  std::unordered_map<std::string, std::string> ret;
  ret["a"] = "1";
  ret["b"] = "2";
  return ret;
}

const ByteBuffer& TestType::GetByteBuffer() const {
  return buffer;
}

std::string TestType::ToPrettyString(Any anything) const {
  LocalVar<JsValue> value = anything.ToJsValue();
  return Console::ConvertToPrettyString(value);
}

TestTypeFactory::TestTypeFactory() {
  AddMemberFunction("acceptNumber", &TestType::AcceptNumber);
  AddMemberFunction("acceptBoolean", &TestType::AcceptBoolean);
  AddMemberFunction("acceptString", &TestType::AcceptString);
  AddMemberFunction("acceptOptionalString", &TestType::AcceptOptionalString);
  AddMemberFunction("acceptOptionalStruct", &TestType::AcceptOptionalStruct);
  AddMemberFunction("acceptIntOrStruct", &TestType::AcceptIntOrStruct);
  AddMemberFunction("acceptStringEnumOrAnyNumber",
                    &TestType::AcceptStringEnumOrAnyNumber);
  AddMemberFunction("acceptStruct", &TestType::AcceptStruct);
  AddMemberFunction("acceptNumberEnum", &TestType::AcceptNumberEnum);
  AddMemberFunction("acceptStringEnum", &TestType::AcceptStringEnum);
  AddMemberFunction("acceptArrayOfStrings", &TestType::AcceptArrayOfStrings);
  AddMemberFunction("acceptCallback", &TestType::AcceptCallback);
  AddMemberFunction("acceptAnything", &TestType::AcceptAnything);
  AddMemberFunction("acceptByteBuffer", &TestType::AcceptByteBuffer);

  AddMemberFunction("isExpectedString", &TestType::IsExpectedString);
  AddMemberFunction("isOptionalPresent", &TestType::IsOptionalPresent);
  AddMemberFunction("isExpectedIntWithOr", &TestType::IsExpectedIntWithOr);
  AddMemberFunction("isExpectedStructWithOr",
                    &TestType::IsExpectedStructWithOr);
  AddMemberFunction("isExpectedConvertedStruct",
                    &TestType::IsExpectedConvertedStruct);
  AddMemberFunction("isConvertedStructEmpty",
                    &TestType::IsConvertedStructEmpty);
  AddMemberFunction("isExpectedNumberEnum", &TestType::IsExpectedNumberEnum);
  AddMemberFunction("isExpectedStringEnum", &TestType::IsExpectedStringEnum);
  AddMemberFunction("isExpectedArrayOfStrings",
                    &TestType::IsExpectedArrayOfStrings);
  AddMemberFunction("isExpectedStringWithAny",
                    &TestType::IsExpectedStringWithAny);
  AddMemberFunction("isTruthy", &TestType::IsTruthy);

  AddMemberFunction("invokeCallbackWithString",
                    &TestType::InvokeCallbackWithString);
  AddMemberFunction("storeByteBuffer", &TestType::StoreByteBuffer);
  AddMemberFunction("changeStringField", &TestType::ChangeStringField);

  AddMemberFunction("throwException", &TestType::ThrowException);

  AddMemberFunction("promiseAcceptString", &TestType::PromiseAcceptString);
  AddMemberFunction("promiseResolveWith", &TestType::PromiseResolveWith);
  AddMemberFunction("promiseResolveAfter", &TestType::PromiseResolveAfter);

  AddMemberFunction("getString", &TestType::GetString);
  AddMemberFunction("getOptionalString", &TestType::GetOptionalString);
  AddMemberFunction("getIntOrString", &TestType::GetIntOrString);
  AddMemberFunction("getStruct", &TestType::GetStruct);
  AddMemberFunction("getNumberEnum", &TestType::GetNumberEnum);
  AddMemberFunction("getStringEnum", &TestType::GetStringEnum);
  AddMemberFunction("getArrayOfStrings", &TestType::GetArrayOfStrings);
  AddMemberFunction("getMapOfStrings", &TestType::GetMapOfStrings);
  AddMemberFunction("getByteBuffer", &TestType::GetByteBuffer);

  AddMemberFunction("toPrettyString", &TestType::ToPrettyString);

  AddReadWriteProperty("optionalObject", &TestType::optional_object);
  AddReadWriteProperty("intOrObject", &TestType::int_or_object);
  AddReadWriteProperty("struct", &TestType::struct_);
  AddReadWriteProperty("array", &TestType::array);
  AddReadWriteProperty("callback", &TestType::callback);
  AddReadWriteProperty("any", &TestType::any);
  AddReadWriteProperty("buffer", &TestType::buffer);
}

}  // namespace js
}  // namespace shaka
