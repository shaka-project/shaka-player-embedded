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

#include "src/mapping/byte_string.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "src/mapping/js_wrappers.h"

namespace shaka {

ByteString::ByteString(const char* source)
    : vector(source, source + strlen(source)) {}

ByteString::ByteString(const std::string& source)
    : vector(source.begin(), source.end()) {}


bool ByteString::TryConvert(Handle<JsValue> value) {
#if defined(USING_V8)
  if (value.IsEmpty() || !value->IsString())
    return false;

  v8::String::Value value_raw(GetIsolate(), value);
  const uint16_t* data = *value_raw;
  const size_t length = value_raw.length();
  if (length == 0 && value.As<v8::String>()->Length() != 0)
    return false;
#elif defined(USING_JSC)
  JSContextRef cx = GetContext();
  if (!value || !JSValueIsString(cx, value))
    return false;

  // TODO: Avoid this copy if possible.
  LocalVar<JsString> str(JSValueToStringCopy(cx, value, nullptr));
  if (!str)
    return false;
  const uint16_t* data = JSStringGetCharactersPtr(str);
  const size_t length = JSStringGetLength(str);
#endif

  std::vector<uint8_t> results(length);
  DCHECK_EQ(results.size(), length);
  for (size_t i = 0; i < length; i++) {
    if (data[i] > 0xFF) {
      LOG(WARNING) << "The string to be encoded contains characters outside "
                      "the Latin1 range.";
      return false;
    }
    results[i] = static_cast<uint8_t>(data[i]);
  }
  swap(results);
  return true;
}

ReturnVal<JsValue> ByteString::ToJsValue() const {
#if defined(USING_V8)
  return v8::String::NewFromOneByte(GetIsolate(), data(),
                                    v8::NewStringType::kNormal, size())
      .ToLocalChecked();
#elif defined(USING_JSC)
  std::unique_ptr<uint16_t[]> temp_data(new uint16_t[size()]);
  // Copy the data by writing each source byte to its own 16-bit element.
  std::copy(data(), data() + size(), temp_data.get());
  LocalVar<JsString> str(JSStringCreateWithCharacters(temp_data.get(), size()));
  CHECK(str);
  return JSValueMakeString(GetContext(), str);
#endif
}

}  // namespace shaka
