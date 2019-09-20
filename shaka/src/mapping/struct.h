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

#ifndef SHAKA_EMBEDDED_MAPPING_STRUCT_H_
#define SHAKA_EMBEDDED_MAPPING_STRUCT_H_

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "src/mapping/convert_js.h"
#include "src/mapping/generic_converter.h"
#include "src/mapping/js_wrappers.h"
#include "src/memory/heap_tracer.h"
#include "src/util/templates.h"

namespace shaka {

// Gets the class for |*this|.  Normally to get a pointer to member, you
// have to use |&MyType::member|, this allows us to avoid passing the current
// type name to the macro |&THIS_TYPE::member|.
#define THIS_TYPE std::decay<decltype(*this)>::type

#define ADD_NAMED_DICT_FIELD(Type, member, name) \
  Type member = CreateFieldConverter(name, &THIS_TYPE::member)
#define ADD_DICT_FIELD(type, member) ADD_NAMED_DICT_FIELD(type, member, #member)

#define DECLARE_STRUCT_SPECIAL_METHODS(Type) \
  static std::string name() {                \
    return #Type;                            \
  }                                          \
  Type();                                    \
  Type(const Type&);                         \
  Type(Type&&);                              \
  ~Type() override;                          \
  Type& operator=(const Type&);              \
  Type& operator=(Type&&)
#define DEFINE_STRUCT_SPECIAL_METHODS(Type)     \
  Type::Type() {}                               \
  Type::Type(const Type&) = default;            \
  Type::Type(Type&&) = default;                 \
  Type::~Type() {}                              \
  Type& Type::operator=(const Type&) = default; \
  Type& Type::operator=(Type&&) = default

class Struct;

namespace impl {

/**
 * Defines a base class for a field converter.  This represents a member of
 * a struct and will convert the JavaScript object member to the respective
 * C++ object member, and vice versa.
 *
 * There needs to be a non-templated base class because we store a vector of
 * them in Struct and they have different (and only known in
 * CreateFieldMember) types.
 */
class FieldConverterBase {
 public:
  virtual ~FieldConverterBase() {}

  /**
   * Search the given object for a property with the name of the field this
   * is converting.  If found, try to convert it and store in the field.
   */
  virtual void SearchAndStore(Struct* dict, Handle<JsObject> object) = 0;
  /** Stores the value of the field in the given object. */
  virtual void AddToObject(const Struct* dict,
                           Handle<JsObject> object) const = 0;
  /** Traces the member on the given object. */
  virtual void Trace(const Struct* dict, memory::HeapTracer* tracer) const = 0;
};

/**
 * The implementation of the field converter.  This will convert a field of
 * type Field that is defined on the type Parent.
 */
template <typename Parent, typename Field>
class FieldConverter : public FieldConverterBase {
 public:
  FieldConverter(const std::string& name, Field Parent::*member)
      : name_(name), member_(member) {}


  void SearchAndStore(Struct* dict, Handle<JsObject> object) override {
    auto parent = static_cast<Parent*>(dict);
    LocalVar<JsValue> member(GetMemberRaw(object, name_));
    (void)FromJsValue(member, &(parent->*member_));
  }

  void AddToObject(const Struct* dict, Handle<JsObject> object) const override {
    auto parent = static_cast<const Parent*>(dict);
    LocalVar<JsValue> value(ToJsValue(parent->*member_));
    SetMemberRaw(object, name_, value);
  }

  void Trace(const Struct* dict, memory::HeapTracer* tracer) const override {
    auto parent = static_cast<const Parent*>(dict);
    tracer->Trace(&(parent->*member_));
  }

 private:
  std::string name_;
  // Store as a pointer to member so if we are copied, we don't need to update
  // the pointers (or make the type non-copyable).
  Field Parent::*member_;
};

}  // namespace impl


/**
 * Defines the base class for struct objects.  These are JavaScript
 * objects that have specific members.  Passing in any JavaScript object is
 * valid and the respective members will be converted.  Any extra members will
 * not be copied.
 *
 * This is a forgiving type, namely it will try to convert each field if
 * possible.  If it is not possible, the field will be the default.  However,
 * if the argument is not an object, it is invalid.  This is non-nullable, so
 * to accept null, use optional<Struct>.
 *
 * In addition to deriving from this type, a Struct must also have a
 * static |name| method.  This is similar to BackingObjects, but you should NOT
 * use DECLARE_TYPE_INFO.  It is required to have a default (argument-less)
 * constructor which can be implicit or user defined.  If it is defined you
 * CANNOT have member initialization (field assignments before the '{'), you
 * MUST use assignment within the constructor body, otherwise the field will
 * not be registered (see the macro above).
 */
class Struct : public GenericConverter, public memory::Traceable {
 public:
  Struct();
  ~Struct() override;

  // Chromium style requires out-of-line copy/move constructors, even though
  // they are still the default.
  Struct(const Struct&);
  Struct(Struct&&);
  Struct& operator=(const Struct&);
  Struct& operator=(Struct&&);

  bool TryConvert(Handle<JsValue> value) override;
  ReturnVal<JsValue> ToJsValue() const override;
  void Trace(memory::HeapTracer* tracer) const override;

 protected:
  template <typename Parent, typename Field>
  Field CreateFieldConverter(const std::string& name, Field Parent::*field) {
    static_assert(std::is_base_of<Struct, Parent>::value,
                  "Must be derived from Struct");
    auto convert = new impl::FieldConverter<Parent, Field>(name, field);
    converters_.emplace_back(convert);
    return Field();
  }

 private:
  std::vector<std::shared_ptr<impl::FieldConverterBase>> converters_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MAPPING_STRUCT_H_
