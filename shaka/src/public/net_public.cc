// Copyright 2020 Google LLC
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

#include "shaka/net.h"
#include "src/core/ref_ptr.h"
#include "src/js/net.h"
#include "src/mapping/js_utils.h"
#include "src/memory/object_tracker.h"

namespace shaka {

class Request::Impl {
 public:
  RefPtr<js::Request> request;
};

Request::~Request() {}

const uint8_t* Request::body() const {
  if (!impl_->request->body.has_value())
    return nullptr;
  return impl_->request->body->data();
}

size_t Request::body_size() const {
  if (!impl_->request->body.has_value())
    return 0;
  return impl_->request->body->size();
}

void Request::SetBodyCopy(const uint8_t* data, size_t size) {
  if (data) {
    if (impl_->request->body.has_value())
      impl_->request->body->SetFromBuffer(data, size);
    else
      impl_->request->body.emplace(data, size);
  } else {
    impl_->request->body.reset();
  }
}

Request::Request(js::Request&& request)
    : uris(request.uris),
      method(request.method),
      headers(request.headers),
      impl_(new Impl{MakeJsRef<js::Request>(std::move(request))}) {}

void Request::Finalize() {
  impl_->request->uris = uris;
  impl_->request->method = method;
  impl_->request->headers = headers;
  // Call ToJsValue to make the Struct update the JS object.
  (void)impl_->request->ToJsValue();
}


class Response::Impl {
 public:
  RefPtr<js::Response> response;
};

Response::~Response() {}

const uint8_t* Response::data() const {
  return impl_->response->data.data();
}

size_t Response::data_size() const {
  return impl_->response->data.size();
}

void Response::SetDataCopy(const uint8_t* data, size_t size) {
  impl_->response->data.SetFromBuffer(data, size);
}

Response::Response()
    : timeMs(0), fromCache(false), impl_(new Impl{MakeJsRef<js::Response>()}) {}

Response::Response(js::Response&& response)
    : uri(response.uri),
      originalUri(response.originalUri),
      headers(response.headers),
      timeMs(response.timeMs),
      fromCache(response.fromCache),
      impl_(new Impl{MakeJsRef<js::Response>(std::move(response))}) {}

void Response::Finalize() {
  impl_->response->uri = uri;
  impl_->response->originalUri = originalUri;
  impl_->response->headers = headers;
  impl_->response->timeMs = timeMs;
  impl_->response->fromCache = fromCache;
  // Call ToJsValue to make the Struct update the JS object.
  (void)impl_->response->ToJsValue();
}

js::Response* Response::JsObject() {
  return impl_->response.get();
}

}  // namespace shaka
