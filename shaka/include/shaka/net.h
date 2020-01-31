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

#ifndef SHAKA_EMBEDDED_NET_H_
#define SHAKA_EMBEDDED_NET_H_

#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "error.h"
#include "macros.h"
#include "optional.h"

namespace shaka {

namespace js {
struct Request;
struct Response;
}  // namespace js

/**
 * The type of request being made.  See shaka.net.NetworkingEngine.RequestType.
 */
enum class RequestType : int8_t {
  Unknown = -1,

  Manifest = 0,
  Segment = 1,
  License = 2,
  App = 3,
  Timing = 4,
};

/**
 * Defines a network request.  This is passed to one or more request filters
 * that may alter the request, then it is passed to a scheme plugin which
 * performs the actual operation.
 *
 * @ingroup player
 */
class SHAKA_EXPORT Request final {
 public:
  ~Request();

  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(Request);

  /**
   * An array of URIs to attempt.  They will be tried in the order they are
   * given.
   */
  std::vector<std::string> uris;

  /** The HTTP method to use for the request. */
  std::string method;

  /** A mapping of headers for the request. */
  std::unordered_map<std::string, std::string> headers;


  /** @return The body of the request, or nullptr if no body. */
  const uint8_t* body() const;

  /** @return The number of bytes in @a body. */
  size_t body_size() const;

  /**
   * Sets the body of the request to a copy of the given data.  Can be called
   * with nullptr to not send any data.
   */
  void SetBodyCopy(const uint8_t* data, size_t size);

 private:
  friend class JsManager;
  friend class Player;
  Request(js::Request&& request);

  void Finalize();

  class Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * Defines a response object.  This includes the response data and header info.
 * This is given back from the scheme plugin.  This is passed to a response
 * filter before being returned from the request call.
 *
 * @ingroup player
 */
class SHAKA_EXPORT Response final {
 public:
  ~Response();

  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(Response);

  /**
   * The URI which was loaded.  Request filters and server redirects can cause
   * this to be different from the original request URIs.
   */
  std::string uri;

  /**
   * The original URI passed to the browser for networking. This is before any
   * redirects, but after request filters are executed.
   */
  std::string originalUri;

  /**
   * A map of response headers, if supported by the underlying protocol.
   * All keys should be lowercased.
   * For HTTP/HTTPS, may not be available cross-origin.
   */
  std::unordered_map<std::string, std::string> headers;

  /**
   * Optional.  The time it took to get the response, in miliseconds.  If not
   * given, NetworkingEngine will calculate it using Date.now.
   */
  optional<double> timeMs;

  /**
   * Optional. If true, this response was from a cache and should be ignored
   * for bandwidth estimation.
   */
  optional<bool> fromCache;


  /** @return The data of the response. */
  const uint8_t* data() const;

  /** @return The number of bytes in @a data. */
  size_t data_size() const;

  /** Sets the body of the response to a copy of the given data. */
  void SetDataCopy(const uint8_t* data, size_t size);

 private:
  friend class JsManager;
  friend class Player;
  Response();
  Response(js::Response&& response);

  void Finalize();
  js::Response* JsObject();

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_NET_H_
