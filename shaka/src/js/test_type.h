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

#ifndef SHAKA_EMBEDDED_JS_TEST_TYPE_H_
#define SHAKA_EMBEDDED_JS_TEST_TYPE_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "shaka/optional.h"
#include "shaka/variant.h"
#include "src/core/member.h"
#include "src/mapping/any.h"
#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/byte_buffer.h"
#include "src/mapping/callback.h"
#include "src/mapping/enum.h"
#include "src/mapping/promise.h"
#include "src/mapping/struct.h"

namespace shaka {
namespace js {

// A Struct type that is used by TestType.
struct TestTypeOptions : public Struct {
  DECLARE_STRUCT_SPECIAL_METHODS_COPYABLE(TestTypeOptions);

  ADD_DICT_FIELD(string, std::string);
  ADD_DICT_FIELD(boolean, bool);
  ADD_DICT_FIELD(any, Any);
};

enum class TestNumberEnum {
  FIRST = 1,
  SECOND,
};

enum class TestStringEnum {
  EMPTY,
  AUTO,
  OTHER,
};

constexpr const int EXPECTED_INT = 123;
constexpr const TestNumberEnum EXPECTED_NUMBER_ENUM = TestNumberEnum::SECOND;
constexpr const TestStringEnum EXPECTED_STRING_ENUM = TestStringEnum::OTHER;
// Used to verify that Unicode characters and embedded nulls are converted
// correctly.
constexpr const char EXPECTED_STRING[] = "ab\xe2\x8d\x85_\0_\xf0\x90\x90\xb7!";


/**
 * Defines a backing type that is used to test the registering framework.
 * Methods are called in JavaScript tests to test the conversion functions.
 */
class TestType : public BackingObject {
  DECLARE_TYPE_INFO(TestType);

 public:
  TestType();

  static TestType* Create() {
    return new TestType();
  }

  void Trace(memory::HeapTracer* tracer) const override;

  void AcceptNumber(double) const {}
  void AcceptBoolean(bool) const {}
  void AcceptString(const std::string&) const {}
  void AcceptOptionalString(optional<std::string>) const {}
  void AcceptOptionalStruct(optional<TestTypeOptions>) const {}
  void AcceptIntOrStruct(variant<int, TestTypeOptions>) const {}
  void AcceptStringEnumOrAnyNumber(variant<TestStringEnum, double>) const {}
  void AcceptStruct(TestTypeOptions) const {}
  void AcceptNumberEnum(TestNumberEnum) const {}
  void AcceptStringEnum(TestStringEnum) const {}
  void AcceptArrayOfStrings(std::vector<std::string>) const {}
  void AcceptCallback(Callback) const {}
  void AcceptAnything(Any) const {}
  void AcceptByteBuffer(ByteBuffer) const {}

  // These return true if the given value is an expected value (the constants
  // above).
  bool IsExpectedString(const std::string& arg) const;
  bool IsOptionalPresent(optional<std::string> arg) const;
  bool IsExpectedIntWithOr(variant<int, Any> arg) const;
  bool IsExpectedStructWithOr(variant<int, TestTypeOptions> arg) const;
  bool IsExpectedConvertedStruct(TestTypeOptions opts) const;
  bool IsConvertedStructEmpty(TestTypeOptions opts) const;
  bool IsExpectedNumberEnum(TestNumberEnum e) const;
  bool IsExpectedStringEnum(TestStringEnum e) const;
  bool IsExpectedArrayOfStrings(const std::vector<std::string>& data) const;
  bool IsExpectedStringWithAny(Any anything) const;
  bool IsTruthy(Any anything) const;

  void InvokeCallbackWithString(Callback callback) const;
  void StoreByteBuffer(ByteBuffer buffer);
  TestTypeOptions ChangeStringField(TestTypeOptions opts);

  ExceptionOr<void> ThrowException(const std::string& message) const;

  Promise PromiseAcceptString(const std::string& value) const;
  Promise PromiseResolveWith(Any value) const;
  Promise PromiseResolveAfter(uint64_t delay) const;

  std::string GetString() const;
  optional<std::string> GetOptionalString(bool has_value) const;
  variant<int, std::string> GetIntOrString(bool get_int) const;
  TestTypeOptions GetStruct() const;
  TestNumberEnum GetNumberEnum() const;
  TestStringEnum GetStringEnum() const;
  std::vector<std::string> GetArrayOfStrings() const;
  std::unordered_map<std::string, std::string> GetMapOfStrings() const;
  const ByteBuffer& GetByteBuffer() const;

  std::string ToPrettyString(Any anything) const;

  optional<Any> optional_object;
  variant<int, Any> int_or_object;
  TestTypeOptions struct_;
  std::vector<Any> array;
  Callback callback;
  Any any;
  ByteBuffer buffer;
};

class TestTypeFactory : public BackingObjectFactory<TestType> {
 public:
  TestTypeFactory();
};

}  // namespace js
}  // namespace shaka

CONVERT_ENUM_AS_NUMBER(shaka::js, TestNumberEnum);

DEFINE_ENUM_MAPPING(shaka::js, TestStringEnum) {
  AddMapping(Enum::EMPTY, "");
  AddMapping(Enum::AUTO, "auto");
  AddMapping(Enum::OTHER, "other");
}

#endif  // SHAKA_EMBEDDED_JS_TEST_TYPE_H_
