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

/**
 * Defines an interface for network scheme plugins.  These are used by Shaka
 * Player to make network requests.  Requests can be completed asynchronously by
 * returning a <code>std::future</code> instance.  This may be called while an
 * asynchronous request is still completing, but won't be called concurrently.
 * This is called on the JS main thread, so it is preferable to avoid lots of
 * work and do it asynchronously.
 *
 * @ingroup player
 */
class SHAKA_EXPORT SchemePlugin {
 public:
  SHAKA_DECLARE_INTERFACE_METHODS(SchemePlugin);

  /**
   * Defines an interface for event listeners.  These should be called by the
   * plugin as part of the download.  These can be called synchronously within
   * the calls, or on any thread while a request is being made.
   */
  class Client {
   public:
    SHAKA_DECLARE_INTERFACE_METHODS(Client);

    /**
     * Called periodically to report progress of asynchronous downloads.
     * @param time The time (in milliseconds) this report covers.
     * @param bytes The number of bytes downloaded in @a time.
     * @param remaining The number of bytes remaining; can be 0 for unknown.
     */
    virtual void OnProgress(double time, uint64_t bytes,
                            uint64_t remaining) = 0;
  };


  /**
   * Called when the player wants to make a network request.  This is expected
   * to read the Request object and load the data into the @a response object.
   * The objects will remain valid until the returned future resolves.
   *
   * @param uri The current URI of the request.
   * @param type The type of request.
   * @param request The request info.
   * @param client A client object used to send events to.
   * @param response The object to fill with response data.
   * @return A future for when the request is finished.
   */
  virtual std::future<optional<Error>> OnNetworkRequest(const std::string& uri,
                                                        RequestType type,
                                                        const Request& request,
                                                        Client* client,
                                                        Response* response) = 0;
};

/**
 * Defines an interface for request/response filters.  These are used by Shaka
 * Player as part of making a network request.  These allow modifying the
 * request/response before handing it off to other pieces.  This is only used for
 * MSE playback, this doesn't affect src= playback.
 *
 * These can be completed asynchronously by returning a <code>std::future</code>
 * instance.  This may be called while an asynchronous request is still
 * completing, but won't be called concurrently.  This is called on the JS main
 * thread, so it is preferable to avoid lots of work and do it asynchronously.
 */
class SHAKA_EXPORT NetworkFilters {
 public:
  SHAKA_DECLARE_INTERFACE_METHODS(NetworkFilters);

  /**
   * Called before a request is sent.  This can modify the request object to
   * change properties of the request.
   *
   * @param type The type of the request.
   * @param request The request object.  This can be modified by the callback
   *   and remains valid until the returned future is resolved.
   * @return A future for when this filter completes.
   */
  virtual std::future<optional<Error>> OnRequestFilter(RequestType type,
                                                       Request* request);

  /**
   * Called after a request sent, but before it is handled by the library.  This
   * can modify the response object.
   *
   * @param type The type of the request.
   * @param response The response object.  This can be modified by the callback
   *   and remains valid until the returned future is resolved.
   * @return A future for when this filter completes.
   */
  virtual std::future<optional<Error>> OnResponseFilter(RequestType type,
                                                        Response* response);
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_NET_H_
