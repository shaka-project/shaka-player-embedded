// Copyright 2017 Google LLC
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

#ifndef SHAKA_EMBEDDED_JS_EME_SEARCH_REGISTRY_H_
#define SHAKA_EMBEDDED_JS_EME_SEARCH_REGISTRY_H_

#include <string>
#include <vector>

#include "shaka/eme/configuration.h"
#include "src/core/ref_ptr.h"
#include "src/js/eme/media_key_system_configuration.h"
#include "src/mapping/promise.h"
#include "src/memory/heap_tracer.h"

namespace shaka {
namespace js {
namespace eme {

/**
 * A task type that searches the ImplementationRegistry for a compatible
 * implementation and resolves/rejects the given Promise appropriately.
 */
class SearchRegistry {
 public:
  SearchRegistry(Promise promise, std::string key_system,
                 std::vector<MediaKeySystemConfiguration> configs);
  ~SearchRegistry();

  SearchRegistry(const SearchRegistry&) = delete;
  SearchRegistry(SearchRegistry&&);
  SearchRegistry& operator=(const SearchRegistry&) = delete;
  SearchRegistry& operator=(SearchRegistry&&);

  void operator()();

 private:
  RefPtr<Promise> promise_;
  std::string key_system_;
  std::vector<RefPtr<MediaKeySystemConfiguration>> configs_;
};

}  // namespace eme
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_EME_SEARCH_REGISTRY_H_
