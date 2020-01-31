// Copyright 2018 Google LLC
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

#include "shaka/eme/data.h"

#include <glog/logging.h>

#include "src/core/ref_ptr.h"
#include "src/js/eme/media_key_session.h"
#include "src/mapping/byte_buffer.h"
#include "src/mapping/js_utils.h"
#include "src/memory/object_tracker.h"
#include "src/util/macros.h"

namespace shaka {
namespace eme {

class Data::Impl {
 public:
  explicit Impl(ByteBuffer buffer)
      : buffer(MakeJsRef<ByteBuffer>(std::move(buffer))) {}
  ~Impl() {}

  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(Impl);

  RefPtr<ByteBuffer> buffer;
};

Data::Data(Data&& other) = default;
Data::~Data() {}

Data& Data::operator=(Data&& other) = default;

const uint8_t* Data::data() const {
  return impl_->buffer->data();
}

size_t Data::size() const {
  return impl_->buffer->size();
}

Data::Data(ByteBuffer* buffer) : impl_(new Impl(std::move(*buffer))) {}

}  // namespace eme
}  // namespace shaka
