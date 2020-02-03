// Copyright 2019 Google LLC
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

#import "shaka/ShakaPlayerStorage.h"

#include <unordered_map>

#include "shaka/storage.h"
#include "src/js/offline_externs+Internal.h"
#include "src/js/offline_externs.h"
#include "src/public/ShakaPlayer+Internal.h"
#include "src/util/objc_utils.h"

namespace {

class NativeClient : public shaka::Storage::Client {
 public:
  NativeClient() {}

  void set_client(id<ShakaPlayerStorageClient> client) {
    _client = client;
  }


  void OnProgress(shaka::StoredContent stored_content, double progress) override {
    ShakaStoredContent *objc = [[ShakaStoredContent alloc] initWithCpp:stored_content];
    shaka::util::DispatchObjcEvent(_client, @selector(onStorageProgress:withContent:), objc,
                                   progress);
  }

 private:
  __weak id<ShakaPlayerStorageClient> _client;
};

std::unordered_map<std::string, std::string> ToUnorderedMap(
    NSDictionary<NSString *, NSString *> *dict) {
  std::unordered_map<std::string, std::string> ret;
  ret.reserve([dict count]);
  for (NSString *key in dict) {
    ret.emplace(key.UTF8String, dict[key].UTF8String);
  }
  return ret;
}

}  // namespace

@interface ShakaPlayerStorage () {
  NativeClient _client;
  shaka::Storage *_storage;
  std::shared_ptr<shaka::JsManager> _engine;
}

@end

@implementation ShakaPlayerStorage

- (instancetype)init {
  self = [self initWithPlayer:nil];
  return self;
}

- (instancetype)initWithClient:(id<ShakaPlayerStorageClient>)client {
  self = [self initWithPlayer:nil andClient:client];
  return self;
}

- (instancetype)initWithPlayer:(ShakaPlayer *)player {
  self = [self initWithPlayer:player andClient:nil];
  return self;
}

- (instancetype)initWithPlayer:(ShakaPlayer *)player
                     andClient:(id<ShakaPlayerStorageClient>)client {
  if (!(self = [super init]))
    return nil;

  _client.set_client(client);
  _engine = ShakaGetGlobalEngine();
  _storage = new shaka::Storage(_engine.get(), player ? [player playerInstance] : nullptr);

  auto results = _storage->Initialize(&_client);
  if (results.has_error()) {
    // TODO: Log/report error.
    return nil;
  }
  return self;
}

- (void)dealloc {
  delete _storage;
}

- (BOOL)storeInProgress {
  auto results = _storage->GetStoreInProgress();
  if (results.has_error())
    return NO;
  else
    return results.results();
}

- (void)destroyWithBlock:(ShakaPlayerAsyncBlock)block {
  auto future = _storage->Destroy();
  shaka::util::CallBlockForFuture(self, std::move(future), block);
}

- (void)listWithBlock:(void (^)(NSArray<ShakaStoredContent *> *, ShakaPlayerError *))block {
  auto results = _storage->List();
  shaka::util::CallBlockForFuture(self, std::move(results), block);
}

- (void)remove:(NSString *)content_uri withBlock:(ShakaPlayerAsyncBlock)block {
  auto results = _storage->Remove(content_uri.UTF8String);
  shaka::util::CallBlockForFuture(self, std::move(results), block);
}

- (void)removeEmeSessionsWithBlock:(void (^)(BOOL, ShakaPlayerError *))block {
  auto results = _storage->RemoveEmeSessions();
  shaka::util::CallBlockForFuture(self, std::move(results), block);
}

- (void)store:(NSString *)uri withBlock:(void (^)(ShakaStoredContent *, ShakaPlayerError *))block {
  auto results = _storage->Store(uri.UTF8String);
  shaka::util::CallBlockForFuture(self, std::move(results), block);
}

- (void)store:(NSString *)uri
    withAppMetadata:(NSDictionary<NSString *, NSString *> *)data
           andBlock:(void (^)(ShakaStoredContent *, ShakaPlayerError *))block {
  auto results = _storage->Store(uri.UTF8String, ToUnorderedMap(data));
  shaka::util::CallBlockForFuture(self, std::move(results), block);
}


- (void)configure:(const NSString *)namePath withBool:(BOOL)value {
  _storage->Configure(namePath.UTF8String, static_cast<bool>(value));
}

- (void)configure:(const NSString *)namePath withDouble:(double)value {
  _storage->Configure(namePath.UTF8String, value);
}

- (void)configure:(const NSString *)namePath withString:(const NSString *)value {
  _storage->Configure(namePath.UTF8String, value.UTF8String);
}

- (void)configureWithDefault:(const NSString *)namePath {
  _storage->Configure(namePath.UTF8String, shaka::DefaultValue);
}

@end
