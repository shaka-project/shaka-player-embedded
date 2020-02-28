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

#ifndef SHAKA_EMBEDDED_NET_OBJC_H_
#define SHAKA_EMBEDDED_NET_OBJC_H_

#import <Foundation/Foundation.h>

#include "macros.h"

typedef NS_ENUM(NSInteger, ShakaPlayerRequestType) {
  // These have the same values as shaka.net.NetworkingEngine.RequestType and
  // shaka::RequestType.
  ShakaPlayerRequestTypeUnknown = -1,
  ShakaPlayerRequestTypeManifest = 0,
  ShakaPlayerRequestTypeSegment = 1,
  ShakaPlayerRequestTypeLicense = 2,
  ShakaPlayerRequestTypeApp = 3,
  ShakaPlayerRequestTypeTiming = 4
};

NS_ASSUME_NONNULL_BEGIN

/**
 * Defines a network request.  This is passed to one or more request filters
 * that may alter the request, then it is passed to a scheme plugin which
 * performs the actual operation.
 *
 * @ingroup player
 */
SHAKA_EXPORT
@interface ShakaPlayerRequest : NSObject

- (instancetype)init NS_UNAVAILABLE;  // Cannot create external instances.

/**
 * An array of URIs to attempt.  They will be tried in the order they are
 * given.
 */
@property(atomic) NSMutableArray<NSString *> *uris;

/** The HTTP method to use for the request. */
@property(atomic) NSString *method;

/** A mapping of headers for the request. */
@property(atomic) NSMutableDictionary<NSString *, NSString *> *headers;

/** The body of the request, or nil if no body. */
@property(atomic, nullable) NSData *body;

@end


/**
 * Defines a response object.  This includes the response data and header info.
 * This is given back from the scheme plugin.  This is passed to a response
 * filter before being returned from the request call.
 *
 * @ingroup player
 */
SHAKA_EXPORT
@interface ShakaPlayerResponse : NSObject

- (instancetype)init NS_UNAVAILABLE;  // Cannot create external instances.

/**
 * The URI which was loaded.  Request filters and server redirects can cause
 * this to be different from the original request URIs.
 */
@property(atomic) NSString *uri;

/**
 * The original URI passed to the browser for networking. This is before any
 * redirects, but after request filters are executed.
 */
@property(atomic) NSString *originalUri;

/**
 * A map of response headers, if supported by the underlying protocol.
 * All keys should be lowercased.
 * For HTTP/HTTPS, may not be available cross-origin.
 */
@property(atomic) NSMutableDictionary<NSString *, NSString *> *headers;

/**
 * If true, this response was from a cache and should be ignored
 * for bandwidth estimation.
 */
@property(atomic) bool fromCache;

/**
 * Optional.  The time it took to get the response, in milliseconds.  If not
 * given, NetworkingEngine will calculate it using the current time.
 */
@property(atomic, nullable) NSNumber *timeMs;

/** The data of the response. */
@property(atomic, nullable) NSData *data;

@end

NS_ASSUME_NONNULL_END

#endif  // SHAKA_EMBEDDED_NET_OBJC_H_
