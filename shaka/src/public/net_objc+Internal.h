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

#ifndef SHAKA_EMBEDDED_PUBLIC_NET_OBJC_INTERNAL_H_
#define SHAKA_EMBEDDED_PUBLIC_NET_OBJC_INTERNAL_H_

#import "shaka/net.h"
#import "shaka/net_objc.h"

@interface ShakaPlayerRequest(Internal)

- (instancetype)initWithRequest:(const shaka::Request &)request;

- (void)finalize:(shaka::Request *)request;

@end


@interface ShakaPlayerResponse(Internal)

- (instancetype)initWithResponse:(const shaka::Response &)response;

- (void)finalize:(shaka::Response *)response;

@end

#endif  // SHAKA_EMBEDDED_PUBLIC_NET_OBJC_INTERNAL_H_
