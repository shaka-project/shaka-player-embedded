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
               JSObjectGetTypedArrayBytesPtr(cx, object, nullptr)) +
           JSObjectGetTypedArrayByteOffset(cx, object, nullptr);
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

ReturnVal<JsValue> ByteBuffer::ToJsValue(proto::ValueType kind) const {
  ToJsValue();  // Ensure the buffer is available.
  DCHECK(!own_ptr_);
  DCHECK(!buffer_.empty());

#if defined(USING_V8)
  LocalVar<JsObject> local_buffer = buffer_.handle();
  LocalVar<v8::ArrayBuffer> array_buffer;
  size_t start = 0;
  if (local_buffer->IsArrayBuffer()) {
    array_buffer = local_buffer.As<v8::ArrayBuffer>();
  } else {
    DCHECK(array_buffer->IsArrayBufferView());
    LocalVar<v8::ArrayBufferView> view = local_buffer.As<v8::ArrayBufferView>();
    array_buffer = view->Buffer();
    start = view->ByteOffset();
  }

  switch (kind) {
    case proto::ArrayBuffer:
      return array_buffer;
    case proto::DataView:
      return v8::DataView::New(array_buffer, start, size_);
    case proto::Int8Array:
      return v8::Int8Array::New(array_buffer, start, size_);
    case proto::Uint8Array:
      return v8::Uint8Array::New(array_buffer, start, size_);
    case proto::Uint8ClampedArray:
      return v8::Uint8ClampedArray::New(array_buffer, start, size_);
    case proto::Int16Array:
      return v8::Int16Array::New(array_buffer, start, size_ / 2);
    case proto::Uint16Array:
      return v8::Uint16Array::New(array_buffer, start, size_ / 2);
    case proto::Int32Array:
      return v8::Int32Array::New(array_buffer, start, size_ / 4);
    case proto::Uint32Array:
      return v8::Uint32Array::New(array_buffer, start, size_ / 4);
    case proto::Float32Array:
      return v8::Float32Array::New(array_buffer, start, size_ / 4);
    case proto::Float64Array:
      return v8::Float64Array::New(array_buffer, start, size_ / 8);
    default:
      LOG(FATAL) << "Invalid enum value " << kind;
  }
#elif defined(USING_JSC)
  JSContextRef cx = GetContext();
  LocalVar<JsObject> array_buffer = buffer_.handle();
  size_t start = 0;
  auto buffer_type = JSValueGetTypedArrayType(cx, array_buffer, nullptr);
  DCHECK_NE(buffer_type, kJSTypedArrayTypeNone);
  if (buffer_type != kJSTypedArrayTypeArrayBuffer) {
    array_buffer = JSObjectGetTypedArrayBuffer(cx, buffer_.handle(), nullptr);
    start = JSObjectGetTypedArrayByteOffset(cx, buffer_.handle(), nullptr);
  }

  JSTypedArrayType jsc_kind;
  size_t elem_size = 1;
  switch (kind) {
    case proto::ArrayBuffer:
      return array_buffer;
    case proto::Int8Array:
      jsc_kind = kJSTypedArrayTypeInt8Array;
      break;
    case proto::Uint8Array:
      jsc_kind = kJSTypedArrayTypeUint8Array;
      break;
    case proto::Uint8ClampedArray:
      jsc_kind = kJSTypedArrayTypeUint8ClampedArray;
      break;
    case proto::Int16Array:
      jsc_kind = kJSTypedArrayTypeInt16Array;
      elem_size = 2;
      break;
    case proto::Uint16Array:
      jsc_kind = kJSTypedArrayTypeUint16Array;
      elem_size = 2;
      break;
    case proto::Int32Array:
      jsc_kind = kJSTypedArrayTypeInt32Array;
      elem_size = 4;
      break;
    case proto::Uint32Array:
      jsc_kind = kJSTypedArrayTypeUint32Array;
      elem_size = 4;
      break;
    case proto::Float32Array:
      jsc_kind = kJSTypedArrayTypeFloat32Array;
      elem_size = 4;
      break;
    case proto::Float64Array:
      jsc_kind = kJSTypedArrayTypeFloat64Array;
      elem_size = 8;
      break;
    default:
      LOG(FATAL) << "Invalid enum value " << kind;
  }
  return JSObjectMakeTypedArrayWithArrayBufferAndOffset(
      cx, jsc_kind, array_buffer, start, size_ / elem_size, nullptr);
#endif
}

void ByteBuffer::Trace(memory::HeapTracer* tracer) const {
  tracer->Trace(&buffer_);
}

void ByteBuffer::ClearFields() {
  buffer_.reset();
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
