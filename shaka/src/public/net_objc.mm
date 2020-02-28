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

#include "shaka/net_objc.h"

#include "src/public/net_objc+Internal.h"
#include "src/util/objc_utils.h"

using shaka::util::ObjcConverter;

@implementation ShakaPlayerRequest

@synthesize uris;
@synthesize method;
@synthesize headers;
@synthesize body;

- (instancetype)initWithRequest:(const shaka::Request &)request {
  if ((self = [super init])) {
    uris = ObjcConverter<std::vector<std::string>>::ToObjc(request.uris);
    method = ObjcConverter<std::string>::ToObjc(request.method);
    headers = ObjcConverter<std::unordered_map<std::string, std::string>>::ToObjc(request.headers);
    if (request.body_size() != 0)
      body = [[NSData alloc] initWithBytes:request.body() length:request.body_size()];
    else
      body = nil;
  }
  return self;
}

- (void)finalize:(shaka::Request *)request {
  request->uris = ObjcConverter<std::vector<std::string>>::FromObjc(uris);
  request->method = ObjcConverter<std::string>::FromObjc(method);
  request->headers = ObjcConverter<std::unordered_map<std::string, std::string>>::FromObjc(headers);
  if (body)
    request->SetBodyCopy(reinterpret_cast<const uint8_t *>([body bytes]), [body length]);
  else
    request->SetBodyCopy(nullptr, 0);
}

@end


@implementation ShakaPlayerResponse

@synthesize uri;
@synthesize originalUri;
@synthesize headers;
@synthesize fromCache;
@synthesize timeMs;
@synthesize data;

- (instancetype)initWithResponse:(const shaka::Response &)response {
  if ((self = [super init])) {
    uri = ObjcConverter<std::string>::ToObjc(response.uri);
    originalUri = ObjcConverter<std::string>::ToObjc(response.originalUri);
    headers = ObjcConverter<std::unordered_map<std::string, std::string>>::ToObjc(response.headers);
    fromCache = response.fromCache.value_or(false);
    if (response.timeMs.has_value())
      timeMs = [[NSNumber alloc] initWithDouble:*response.timeMs];
    else
      timeMs = nil;
    if (response.data_size() != 0)
      data = [[NSData alloc] initWithBytes:response.data() length:response.data_size()];
    else
      data = nil;
  }
  return self;
}

- (void)finalize:(shaka::Response *)response {
  response->uri = ObjcConverter<std::string>::FromObjc(uri);
  response->originalUri = ObjcConverter<std::string>::FromObjc(originalUri);
  response->headers =
      ObjcConverter<std::unordered_map<std::string, std::string>>::FromObjc(headers);
  response->fromCache = fromCache;
  if (timeMs)
    response->timeMs = [timeMs doubleValue];
  else
    response->timeMs = {};
  if (data)
    response->SetDataCopy(reinterpret_cast<const uint8_t *>([data bytes]), [data length]);
  else
    response->SetDataCopy(nullptr, 0);
}

@end
