# Network Filters

Shaka Player Embedded supports network request and response filters similarly to
the JavaScript Shaka Player project.  Also see
[their tutorial](https://shaka-player-demo.appspot.com/docs/api/tutorial-license-wrapping.html).

Filters are added to either the `shaka::Player` (C++) or `ShakaPlayer`
(Objective-C/Swift) types.  Filters are per-Player and can be different with
different Player instances.  You can also add multiple filters if desired;
filters are called in the order they are registered and are called sequentially.
Filters are implemented as objects that implement a required interface.  There
is a callback for request and response calls.  You don't have to implement both;
if either are missing, a no-op filter will be used.


## Basic Filters

```{.cc}
class MyFilter : public shaka::NetworkFilters {
 public:
  std::future<shaka::optional<shaka::Error>> OnRequestFilter(
      shaka::RequestType type, shaka::Request* request) override {
    if (type == shaka::RequestType::License) {
      request->headers["foo"] = "bar";

      std::vector<uint8_t> body =
          {request->body(), request->body() + request->body_size()};
      WrapRequest(body);  // Process request body if needed.
      request->SetBodyCopy(body.data(), body.size());
    }
    return {};  // Can just use std::future default constructor.
  }
};
```

```{.mm}
\@interface MyFilter : NSObject <ShakaPlayerNetworkFilter>

- (void)onPlayer:(ShakaPlayer *)player
  networkRequest:(ShakaPlayerRequest *)request
          ofType:(ShakaPlayerRequestType)type
       withBlock:(ShakaPlayerAsyncBlock)block;

@end

@implementation MyFilter

- (void)onPlayer:(ShakaPlayer *)player
  networkRequest:(ShakaPlayerRequest *)request
          ofType:(ShakaPlayerRequestType)type
       withBlock:(ShakaPlayerAsyncBlock)block {
  if (type == ShakaPlayerRequestTypeLicense) {
    request.headers[@"foo"] = @"bar";
    request.body = WrapRequest(request.body);
  }
  block(nil);  // MUST call block() at some point to continue request.
}

@end
```

```{.swift}
class MyFilter : NSObject, ShakaPlayerNetworkFilter {
  func onPlayer(_ player: ShakaPlayer, networkRequest: ShakaPlayerRequest,
                of type: ShakaPlayerRequestType,
                with block: @escaping ShakaPlayerAsyncBlock) {
    if (type == .license) {
      networkRequest.headers["foo"] = "bar";
      networkRequest.body = WrapRequest(networkRequest.body);
    }
    block(nil);  // MUST call block() at some point to continue request.
  }
};
```


## Asynchronous filters

Network filters can also be completed asynchronously.  This is preferable when
lots of work is needed since the filter callbacks block either the JS main
thread (in C++) or the app main thread (Objective-C/Swift).

In C++, returning a `std::future` value is used to indicate async work.  An
invalid future (e.g. with the default constructor) means the filter completed
synchronously.  Returning a valid future means the filter is still processing.
Once the future resolves, the filter is done.  Apps create a `std::future`
instance by using `std::async` or `std::promise`.

```{.cc}
std::future<shaka::optional<shaka::Error>> OnRequestFilter(
    shaka::RequestType type, shaka::Request* request) {
  // Create a shared_ptr since std::promise isn't copyable.
  auto promise =
      std::make_shared<std::promise<shaka::optional<shaka::Error>>>();

  // Do something on a background thread/worker:
  std::thread t([=]() {
    sleep(10);

    // Request pointer remains valid until std::future resolves.
    request->headers["foo"] = "bar";

    // Use shaka::optional default constructor to indicate success.
    promise->set_value({});
  });
  t.detach();

  // Return the future so the main thread can continue.
  return promise->get_future();
}
```

For Objective-C/Swift, you MUST call the `block` argument when the filter is
done.  This can be done synchronously within the method, or you can call it
later if the filter isn't done yet.  Forgetting to call the method will cause
the request to hang forever.

```{.mm}
- (void)onPlayer:(ShakaPlayer *)player
  networkRequest:(ShakaPlayerRequest *)request
          ofType:(ShakaPlayerRequestType)type
       withBlock:(ShakaPlayerAsyncBlock)block {
  // Do something on a background queue.
  long priority = DISPATCH_QUEUE_PRIORITY_DEFAULT;
  dispatch_async(dispatch_get_global_queue(priority, 0), ^{
    request.headers[@"foo"] = @"bar";

    // Signal we're done; can be done on any queue/thread.
    block(nil);
  });
}
```


## Reporting Errors

It is possible for you to report an error from the filter.  Doing this will fail
the network request.  This may result in retries, or it may result in a fatal
error.  Apps should not use "normal" exceptions to report errors; instead they
should create an instance of either `shaka::Error` (C++) or `ShakaPlayerError`
(Objective-C/Swift).

```{.cc}
std::future<shaka::optional<shaka::Error>> OnRequestFilter(
    shaka::RequestType type, shaka::Request* request) {
  std::promise<shaka::optional<shaka::Error>> promise;

  // Set the error, can be done on background threads/workers.
  promise.set_value(shaka::Error("Some error happened"));

  return promise.get_future();
}
```

```{.mm}
- (void)onPlayer:(ShakaPlayer *)player
  networkRequest:(ShakaPlayerRequest *)request
          ofType:(ShakaPlayerRequestType)type
       withBlock:(ShakaPlayerAsyncBlock)block {
  ShakaPlayerError *error =
      [[ShakaPlayerError alloc] initWithMessage:@"Something happened"];
  block(error);
}
```
