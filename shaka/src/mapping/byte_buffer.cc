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

#include "src/mapping/byte_buffer.h"

#include <utility>

#include "src/memory/heap_tracer.h"

namespace shaka {

namespace {
#ifdef USING_V8
uint8_t* GetDataPointer(v8::Local<v8::ArrayBuffer> buffer) {
  return reinterpret_cast<uint8_t*>(buffer->GetContents().Data());
}
#elif defined(USING_JSC)
void FreeData(void* data, void*) {
  std::free(data);  // NOLINT
}
#endif
}  // namespace

ByteBuffer::ByteBuffer() {}

ByteBuffer::ByteBuffer(const uint8_t* data, size_t data_size) {
  SetFromBuffer(data, data_size);
}

ByteBuffer::ByteBuffer(ByteBuffer&& other)
    : buffer_(std::move(other.buffer_)),
      ptr_(other.ptr_),
      size_(other.size_),
      own_ptr_(other.own_ptr_) {
  other.ClearFields();
}

ByteBuffer::~ByteBuffer() {
  Clear();
}

ByteBuffer& ByteBuffer::operator=(ByteBuffer&& other) {
  Clear();

  buffer_ = std::move(other.buffer_);
  ptr_ = other.ptr_;
  size_ = other.size_;
  own_ptr_ = other.own_ptr_;

  other.ClearFields();
  return *this;
}


void ByteBuffer::Clear() {
  if (own_ptr_)
    std::free(ptr_);  // NOLINT
  ClearFields();
}

void ByteBuffer::SetFromDynamicBuffer(const util::DynamicBuffer& other) {
  ClearAndAllocateBuffer(other.Size());
  other.CopyDataTo(ptr_, size_);
}

void ByteBuffer::SetFromBuffer(const void* buffer, size_t size) {
  ClearAndAllocateBuffer(size);
  std::memcpy(ptr_, buffer, size_);
}

bool ByteBuffer::TryConvert(Handle<JsValue> value) {
#if defined(USING_V8)
  if (value.IsEmpty())
    return false;

  if (value->IsArrayBuffer()) {
    v8::Local<v8::ArrayBuffer> buffer = value.As<v8::ArrayBuffer>();
    ptr_ = GetDataPointer(buffer);
    size_ = buffer->ByteLength();
  } else if (value->IsArrayBufferView()) {
    v8::Local<v8::ArrayBufferView> view = value.As<v8::ArrayBufferView>();
    ptr_ = GetDataPointer(view->Buffer()) + view->ByteOffset();
    size_ = view->ByteLength();
  } else {
    return false;
  }

  buffer_ = value.As<v8::Object>();
#elif defined(USING_JSC)
  JSContextRef cx = GetContext();
  auto type = JSValueGetTypedArrayType(cx, value, nullptr);
  if (type == kJSTypedArrayTypeNone)
    return false;

  LocalVar<JsObject> object = UnsafeJsCast<JsObject>(value);
  if (type == kJSTypedArrayTypeArrayBuffer) {
    ptr_ = reinterpret_cast<uint8_t*>(
        JSObjectGetArrayBufferBytesPtr(cx, object, nullptr));
    size_ = JSObjectGetArrayBufferByteLength(cx, object, nullptr);
  } else {
    ptr_ = reinterpret_cast<uint8_t*>(
        JSObjectGetTypedArrayBytesPtr(cx, object, nullptr));
    size_ = JSObjectGetTypedArrayByteLength(cx, object, nullptr);
  }
  buffer_ = object;
#endif
  own_ptr_ = false;
  return true;
}

ReturnVal<JsValue> ByteBuffer::ToJsValue() const {
  if (buffer_.empty()) {
    DCHECK(own_ptr_);
#if defined(USING_V8)
    buffer_ = v8::ArrayBuffer::New(GetIsolate(), ptr_, size_,
                                   v8::ArrayBufferCreationMode::kInternalized);
#elif defined(USING_JSC)
    buffer_ = Handle<JsObject>(JSObjectMakeArrayBufferWithBytesNoCopy(
        GetContext(), ptr_, size_, &FreeData, nullptr, nullptr));
#endif
    CHECK(!buffer_.empty());
    own_ptr_ = false;
  }
  return buffer_.value();
}

void ByteBuffer::Trace(memory::HeapTracer* tracer) const {
  tracer->Trace(&buffer_);
}

void ByteBuffer::ClearFields() {
  ptr_ = nullptr;
  size_ = 0;
  own_ptr_ = false;
}

void ByteBuffer::ClearAndAllocateBuffer(size_t size) {
  Clear();

  // Use malloc here, the same as in JsEngine::ArrayBufferAllocator.
  // Must also be compatible with JSC (uses free()).
  own_ptr_ = true;
  size_ = size;
  ptr_ = reinterpret_cast<uint8_t*>(std::malloc(size_));  // NOLINT
  CHECK(ptr_);
}

}  // namespace shaka
