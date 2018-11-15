// Copyright 2018 Google LLC
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

#ifndef SHAKA_EMBEDDED_EME_PROMISE_H_
#define SHAKA_EMBEDDED_EME_PROMISE_H_

#include <memory>
#include <string>

#include "configuration.h"

namespace shaka {

class Promise;

namespace js {
namespace eme {
class MediaKeys;
class MediaKeySession;
}  // namespace eme
}  // namespace js

namespace eme {

/**
 * Defines a wrapper around a JavaScript Promise object.
 *
 * EME APIs are always given valid Promise objects; but if you use the default
 * constructor or use std::move(), it will create an invalid Promise object.
 * You can't call any methods on an invalid object except for valid().
 *
 * @ingroup eme
 */
class SHAKA_EXPORT EmePromise final {
 public:
  /**
   * Creates an *invalid* Promise object.  The members of this object can't be
   * used unless a valid Promise is moved/copied into this.
   */
  EmePromise();

  EmePromise(const EmePromise& other);
  EmePromise(EmePromise&& other);
  ~EmePromise();

  EmePromise& operator=(const EmePromise& other);
  EmePromise& operator=(EmePromise&& other);

  /** @return Whether this object is valid and can be resolved/rejected. */
  bool valid() const;

  /**
   * Resolves the Promise.  If the Promise has already been resolved/rejected,
   * this call is ignored.
   */
  void Resolve();

  /**
   * Resolves the Promise with the given value.  If the Promise has already been
   * resolved/rejected, this call is ignored.
   */
  void ResolveWith(bool value);

  /**
   * Rejects the Promise with the given error.  If the Promise has already been
   * resolved/rejected, this call is ignored.
   */
  void Reject(ExceptionType except_type, const std::string& message);

 private:
  class Impl;

  friend class ClearKeyImplementationTest;
  friend class js::eme::MediaKeys;
  friend class js::eme::MediaKeySession;
  EmePromise(const Promise& promise, bool has_value);
  EmePromise(std::shared_ptr<Impl> impl);

  std::shared_ptr<Impl> impl_;
};

}  // namespace eme
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_EME_PROMISE_H_
