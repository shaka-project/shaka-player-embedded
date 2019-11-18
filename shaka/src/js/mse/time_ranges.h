// Copyright 2016 Google LLC
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

#ifndef SHAKA_EMBEDDED_JS_MSE_TIME_RANGES_H_
#define SHAKA_EMBEDDED_JS_MSE_TIME_RANGES_H_

#include "src/mapping/backing_object.h"
#include "src/mapping/backing_object_factory.h"
#include "src/mapping/exception_or.h"
#include "src/media/types.h"

namespace shaka {
namespace js {
namespace mse {

class TimeRanges : public BackingObject {
  DECLARE_TYPE_INFO(TimeRanges);

 public:
  explicit TimeRanges(media::BufferedRanges ranges);

  bool IsShortLived() const override;

  uint32_t length() const {
    return ranges_.size();
  }

  ExceptionOr<double> Start(uint32_t index) const;
  ExceptionOr<double> End(uint32_t index) const;

 private:
  media::BufferedRanges ranges_;
};

class TimeRangesFactory : public BackingObjectFactory<TimeRanges> {
 public:
  TimeRangesFactory();
};

}  // namespace mse
}  // namespace js
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_JS_MSE_TIME_RANGES_H_
