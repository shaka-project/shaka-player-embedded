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

#include "src/js/mse/time_ranges.h"

#include <sstream>
#include <stdexcept>

#include "src/js/js_error.h"

namespace shaka {
namespace js {
namespace mse {

namespace {

JsError OutOfRange(uint32_t index, size_t max) {
  std::stringstream ss;
  ss << "The given index " << index << " was greater than the number ";
  ss << "of elements " << max;
  return JsError::DOMException(IndexSizeError, ss.str());
}

}  // namespace

TimeRanges::TimeRanges(media::BufferedRanges ranges) : ranges_(ranges) {}
// \cond Doxygen_Skip
TimeRanges::~TimeRanges() {}
// \endcond Doxygen_Skip

bool TimeRanges::IsShortLived() const {
  return true;
}

ExceptionOr<double> TimeRanges::Start(uint32_t index) const {
  if (index >= ranges_.size())
    return OutOfRange(index, ranges_.size());
  return ranges_[index].start;
}

ExceptionOr<double> TimeRanges::End(uint32_t index) const {
  if (index >= ranges_.size())
    return OutOfRange(index, ranges_.size());
  return ranges_[index].end;
}


TimeRangesFactory::TimeRangesFactory() {
  AddGenericProperty("length", &TimeRanges::length);

  AddMemberFunction("start", &TimeRanges::Start);
  AddMemberFunction("end", &TimeRanges::End);
}

}  // namespace mse
}  // namespace js
}  // namespace shaka
