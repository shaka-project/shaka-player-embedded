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

#include "shaka/eme/implementation_registry.h"

#include <glog/logging.h>

#include <mutex>
#include <unordered_map>

#include "src/util/macros.h"

namespace shaka {
namespace eme {

namespace {

BEGIN_ALLOW_COMPLEX_STATICS
std::unordered_map<std::string, std::shared_ptr<ImplementationFactory>>
    factories_;
std::mutex mutex_;
END_ALLOW_COMPLEX_STATICS

}  // namespace

void ImplementationRegistry::AddImplementation(
    const std::string& key_system,
    std::shared_ptr<ImplementationFactory> factory) {
  std::unique_lock<std::mutex> lock(mutex_);
  const auto it = factories_.find(key_system);
  if (it != factories_.end())
    it->second = factory;
  else
    factories_.emplace(key_system, factory);
}

std::shared_ptr<ImplementationFactory>
ImplementationRegistry::GetImplementation(const std::string& key_system) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = factories_.find(key_system);
  return it != factories_.end() ? it->second : nullptr;
}

}  // namespace eme
}  // namespace shaka
