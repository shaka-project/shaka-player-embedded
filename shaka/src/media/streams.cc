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

#include "shaka/media/streams.h"

#include <glog/logging.h>
#include <stdio.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <list>
#include <unordered_set>
#include <utility>

#include "src/debug/mutex.h"
#include "src/media/media_utils.h"
#include "src/util/macros.h"

namespace shaka {
namespace media {

namespace {

// These exist to reduce the number of if checks for ordering.  This is done
// by using a single if at the beginning of the method and using a function
// pointer to one of these methods.

template <bool OrderByDts = true>
double GetTime(const std::shared_ptr<BaseFrame>& a) {
  return a->dts;
}
template <>
double GetTime<false>(const std::shared_ptr<BaseFrame>& a) {
  return a->pts;
}

#ifndef NDEBUG
template <bool OrderByDts>
bool FrameLessThan(const std::shared_ptr<BaseFrame>& a,
                   const std::shared_ptr<BaseFrame>& b) {
  return GetTime<OrderByDts>(a) < GetTime<OrderByDts>(b);
}
#endif

template <bool OrderByDts>
bool FrameExtendsPast(const std::shared_ptr<BaseFrame>& a,
                      const std::shared_ptr<BaseFrame>& b) {
  return GetTime<OrderByDts>(a) + a->duration + StreamBase::kMaxGapSize >=
         GetTime<OrderByDts>(b);
}

template <typename Iter>
void UpdatePtsRanges(Iter range) {
  DCHECK(!range->frames.empty());
  range->start_pts = HUGE_VAL;
  range->end_pts = -HUGE_VAL;
  for (auto& frame : range->frames) {
    range->start_pts = std::min(range->start_pts, frame->pts);
    range->end_pts = std::max(range->end_pts, frame->pts + frame->duration);
  }
}

/**
 * Returns an iterator to the first element in |list| that is greater than or
 * equal to |frame|.
 *
 * This performs a linear search through |list| to find the element.  This will
 * perform a forward or reverse search depending on which would (likely) be
 * better.
 *
 * If we are inserting a frame at the end, this is O(1) time (usual case),
 * otherwise this is O(n).
 *
 * This returns a non-const iterator because a standard library bug in
 * libstdc++ 4.8 makes std::list::insert require non-const iterators.
 * See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=57158
 */
template <bool OrderByDts>
std::list<std::shared_ptr<BaseFrame>>::iterator FrameLowerBound(
    const std::list<std::shared_ptr<BaseFrame>>& list, double time) {
  auto& mutable_list =
      const_cast<std::list<std::shared_ptr<BaseFrame>>&>(list);  // NOLINT
  if (list.empty())
    return mutable_list.end();

  if (time - GetTime<OrderByDts>(list.front()) <
      GetTime<OrderByDts>(list.back()) - time) {
    // Closer to the front, find the first element that is not less than |time|.
    auto notLessThan = [&time](const std::shared_ptr<BaseFrame>& other) {
      return GetTime<OrderByDts>(other) >= time;
    };
    return std::find_if(mutable_list.begin(), mutable_list.end(), notLessThan);
  }

  // Closer to the end, first use a reverse search to find the first (in
  // reverse order) that is less than |time|.
  auto isLessThan = [&time](const std::shared_ptr<BaseFrame>& other) {
    return GetTime<OrderByDts>(other) < time;
  };
  auto rit =
      std::find_if(mutable_list.rbegin(), mutable_list.rend(), isLessThan);
  // |*rit| is equal to the first element (going backwards) that is less than
  // |frame|.  We want to return |rit - 1| as a forward iterator, namely the
  // first element that is not less than |frame|.  The |base()| of a reverse
  // iterator is the same element as |*(rit - 1)|, so return that.
  // See: http://en.cppreference.com/w/cpp/iterator/reverse_iterator/base
  return rit.base();
}

struct Range {
  Range() {}
  ~Range() {}

  SHAKA_NON_COPYABLE_TYPE(Range);

  std::list<std::shared_ptr<BaseFrame>> frames;

  double start_pts = HUGE_VAL;
  double end_pts = -HUGE_VAL;
};

}  // namespace

class StreamBase::Impl {
 public:
  explicit Impl(bool order_by_dts)
      : mutex("StreamBase"), order_by_dts(order_by_dts) {}

  Mutex mutex;
  std::list<Range> buffered_ranges;
  const bool order_by_dts;
};


StreamBase::StreamBase(bool order_by_dts) : impl_(new Impl(order_by_dts)) {}

StreamBase::~StreamBase() {}

size_t StreamBase::EstimateSize() const {
  std::unique_lock<Mutex> lock(impl_->mutex);

  size_t size = 0;
  for (auto& range : impl_->buffered_ranges) {
    for (auto& frame : range.frames)
      size += frame->EstimateSize();
  }
  return size;
}

void StreamBase::AddFrameInternal(std::shared_ptr<BaseFrame> frame) {
  std::unique_lock<Mutex> lock(impl_->mutex);
  DCHECK(frame);

  auto extendsPast =
      impl_->order_by_dts ? &FrameExtendsPast<true> : &FrameExtendsPast<false>;
  auto lowerBound =
      impl_->order_by_dts ? &FrameLowerBound<true> : &FrameLowerBound<false>;
  auto getTime = impl_->order_by_dts ? &GetTime<true> : &GetTime<false>;

  // Find the first buffered range that ends after |frame|.
  auto range_it =
      std::find_if(impl_->buffered_ranges.begin(), impl_->buffered_ranges.end(),
                   [&](const Range& range) {
                     return extendsPast(range.frames.back(), frame);
                   });

  if (range_it == impl_->buffered_ranges.end()) {
    // |frame| was after every existing range, create a new one.
    impl_->buffered_ranges.emplace_back();
    impl_->buffered_ranges.back().start_pts = frame->pts;
    impl_->buffered_ranges.back().end_pts = frame->pts + frame->duration;
    impl_->buffered_ranges.back().frames.emplace_back(frame);
  } else if (!extendsPast(frame, range_it->frames.front())) {
    // |frame| is before this range, so it starts a new range before this one.
    auto it = impl_->buffered_ranges.emplace(range_it);
    it->start_pts = frame->pts;
    it->end_pts = frame->pts + frame->duration;
    it->frames.emplace_back(frame);
  } else {
    // |frame| is inside the current range.
    auto frame_it = lowerBound(range_it->frames, getTime(frame));
    range_it->start_pts = std::min(range_it->start_pts, frame->pts);
    range_it->end_pts =
        std::max(range_it->end_pts, frame->pts + frame->duration);
    if (frame_it != range_it->frames.end() &&
        getTime(*frame_it) == getTime(frame)) {
      swap(*frame_it, frame);
    } else {
      range_it->frames.insert(frame_it, frame);
    }
  }

  // If the frame closed a gap, then merge the buffered ranges.
  DCHECK_NE(0u, impl_->buffered_ranges.size());
  for (auto it = std::next(impl_->buffered_ranges.begin());
       it != impl_->buffered_ranges.end();) {
    auto prev = std::prev(it);
    if (extendsPast(prev->frames.back(), it->frames.front())) {
      // Move all elements from |it->frames| to the end of |prev->frames|.
      // Since both lists are sorted and |prev < it|, this will remain sorted.
      prev->frames.splice(prev->frames.end(), it->frames);
      prev->start_pts = std::min(prev->start_pts, it->start_pts);
      prev->end_pts = std::max(prev->end_pts, it->end_pts);
      it = impl_->buffered_ranges.erase(it);
    } else {
      it++;
    }
  }

  AssertRangesSorted();
}

std::vector<BufferedRange> StreamBase::GetBufferedRanges() const {
  std::unique_lock<Mutex> lock(impl_->mutex);
  AssertRangesSorted();

  std::vector<BufferedRange> ret;
  ret.reserve(impl_->buffered_ranges.size());
  for (const Range& range : impl_->buffered_ranges) {
    ret.emplace_back(range.start_pts, range.end_pts);
  }
  return ret;
}

size_t StreamBase::CountFramesBetween(double start_time,
                                      double end_time) const {
  std::unique_lock<Mutex> lock(impl_->mutex);
  AssertRangesSorted();

  auto getTime = impl_->order_by_dts ? &GetTime<true> : &GetTime<false>;
  auto lowerBound =
      impl_->order_by_dts ? &FrameLowerBound<true> : &FrameLowerBound<false>;

  // Find the first buffered range that includes or is after |start_time|.
  auto range_it =
      std::find_if(impl_->buffered_ranges.begin(), impl_->buffered_ranges.end(),
                   [&](const Range& range) {
                     return getTime(range.frames.back()) >= start_time;
                   });

  size_t num_frames = 0;
  for (; range_it != impl_->buffered_ranges.end(); range_it++) {
    // |start| points to the frame that starts at or greater than
    // |start_time|.
    auto start = lowerBound(range_it->frames, start_time);
    auto end = lowerBound(range_it->frames, end_time);
    DCHECK(start != range_it->frames.end());
    num_frames += std::distance(start, end);
    if (getTime(*start) == start_time && start != end)
      num_frames--;
    if (end != range_it->frames.end())
      break;
  }

  return num_frames;
}

void StreamBase::Remove(double start, double end) {
  // Note that remove always uses PTS, even when sorting using DTS.  This is
  // intended to work like the MSE definition.

  std::unique_lock<Mutex> lock(impl_->mutex);
  bool is_removing = false;
  for (auto it = impl_->buffered_ranges.begin();
       it != impl_->buffered_ranges.end();) {
    // These represent the range of frames within this buffer to delete.
    auto frame_del_start = is_removing ? it->frames.begin() : it->frames.end();
    auto frame_del_end = it->frames.end();

    for (auto frame = it->frames.begin(); frame != it->frames.end(); frame++) {
      if (!is_removing) {
        // Only start deleting frames whose start time is in the range.
        if ((*frame)->pts >= start && (*frame)->pts < end) {
          is_removing = true;
          frame_del_start = frame;
        }
      } else if ((*frame)->pts >= end && (*frame)->is_key_frame) {
        // The MSE spec says to remove up to the next key frame.
        frame_del_end = frame;
        is_removing = false;
        break;
      }
    }

    if (frame_del_start != it->frames.begin() &&
        frame_del_start != it->frames.end() &&
        frame_del_end != it->frames.end()) {
      // We deleted a partial range, so we need to split the buffered range.
      auto new_it = impl_->buffered_ranges.emplace(it);  // new_it + 1 == it

      // Move the elements before |frame_del_start| from |it->frames| to
      // |new_it->frames|.
      new_it->frames.splice(new_it->frames.end(), it->frames,
                            it->frames.begin(), frame_del_start);

      it->frames.erase(it->frames.begin(), frame_del_end);
      UpdatePtsRanges(it);
      UpdatePtsRanges(new_it);
      it++;  // list::emplace() doesn't invalidate any iterators.
    } else {
      it->frames.erase(frame_del_start, frame_del_end);
      if (it->frames.empty()) {
        it = impl_->buffered_ranges.erase(it);
      } else {
        UpdatePtsRanges(it);
        it++;
      }
    }
  }

  AssertRangesSorted();
}

void StreamBase::Clear() {
  std::unique_lock<Mutex> lock(impl_->mutex);
  impl_->buffered_ranges.clear();
}

void StreamBase::DebugPrint(bool all_frames) {
  std::unique_lock<Mutex> lock(impl_->mutex);
  AssertRangesSorted();

  fprintf(stderr, "Stream order by %s:\n", impl_->order_by_dts ? "DTS" : "PTS");
  if (impl_->buffered_ranges.empty())
    fprintf(stderr, "  Nothing buffered\n");
  size_t range_i = 0;
  for (const Range& range : impl_->buffered_ranges) {
    fprintf(stderr, "  Range[%zu]: %.2f-%.2f\n", range_i, range.start_pts,
            range.end_pts);
    if (all_frames) {
      size_t frame_i = 0;
      for (const auto& frame : range.frames) {
        fprintf(stderr,
                "    Frame[%zu]: is_key_frame=%-5s, pts=%.2f, dts=%.2f\n",
                frame_i, frame->is_key_frame ? "true" : "false", frame->pts,
                frame->dts);
        frame_i++;
      }
    }
    range_i++;
  }
}

std::shared_ptr<BaseFrame> StreamBase::GetFrameInternal(
    double time, FrameLocation kind) const {
  std::unique_lock<Mutex> lock(impl_->mutex);
  AssertRangesSorted();

  auto getTime = impl_->order_by_dts ? &GetTime<true> : &GetTime<false>;
  auto lowerBound =
      impl_->order_by_dts ? &FrameLowerBound<true> : &FrameLowerBound<false>;

  // Find the first buffered range that includes or is after |time|.
  auto it = std::find_if(
      impl_->buffered_ranges.begin(), impl_->buffered_ranges.end(),
      [&](const Range& range) { return getTime(range.frames.back()) >= time; });

  if (it == impl_->buffered_ranges.end()) {
    if (kind == FrameLocation::After || impl_->buffered_ranges.empty())
      return nullptr;

    it = std::prev(impl_->buffered_ranges.end());
  }

  // |frame_it| points to the frame that starts at or greater than |time|.
  auto frame_it = lowerBound(it->frames, time);

  switch (kind) {
    case FrameLocation::After:
      // Find the frame after |time|.
      DCHECK(frame_it != it->frames.end());
      if (getTime(*frame_it) > time)
        return *frame_it;
      else if (std::next(frame_it) != it->frames.end())
        return *std::next(frame_it);
      else if (std::next(it) != impl_->buffered_ranges.end())
        return *std::next(it)->frames.begin();
      else
        return nullptr;

    case FrameLocation::Near: {
      if (frame_it == it->frames.end())
        return it->frames.back();

      // Find the frame before this to see if it is closer.
      auto prev_it = frame_it;
      if (frame_it != it->frames.begin())
        prev_it = std::prev(frame_it);
      else if (it != impl_->buffered_ranges.begin())
        prev_it = std::prev(std::prev(it)->frames.end());

      double prev_diff = time - getTime(*prev_it) - (*prev_it)->duration;
      double diff = getTime(*frame_it) - time;
      return prev_diff < diff && diff != 0 ? *prev_it : *frame_it;
    }

    case FrameLocation::KeyFrameBefore:
      if (frame_it == it->frames.end())
        frame_it = std::prev(it->frames.end());
      else if (frame_it != it->frames.begin() && getTime(*frame_it) > time)
        frame_it--;  // If frame_it is a future frame, move backward.

      DCHECK((*it->frames.begin())->is_key_frame);
      while (!(*frame_it)->is_key_frame)
        frame_it--;

      return getTime(*frame_it) <= time ? *frame_it : nullptr;
  }
}

void StreamBase::AssertRangesSorted() const {
#ifndef NDEBUG
  auto frame_less_than =
      impl_->order_by_dts ? &FrameLessThan<true> : &FrameLessThan<false>;
  auto range_is_valid = [&](const Range& range) {
    // A buffered range must:
    // - Be non-empty.
    // - Start with a key frame.
    // - Have sorted frames.
    CHECK(!range.frames.empty());
    CHECK(range.frames.front()->is_key_frame);
    CHECK_LE(range.start_pts, range.end_pts);
    CHECK(std::is_sorted(range.frames.begin(), range.frames.end(),
                         frame_less_than));
    return true;
  };
  auto range_less_than = [&](const Range& first, const Range& second) {
    // Some versions of the standard library will perform debug checks that will
    // call this with the same argument.  Don't assert in this case.
    if (&first == &second)
      return false;

    // The ranges must not overlap.
    if (first.start_pts < second.start_pts) {
      CHECK_LT(first.end_pts, second.start_pts);
      return true;
    }
    CHECK_GT(first.start_pts, second.end_pts);
    return false;
  };

  CHECK(std::all_of(impl_->buffered_ranges.begin(),
                    impl_->buffered_ranges.end(), range_is_valid));
  CHECK(std::is_sorted(impl_->buffered_ranges.begin(),
                       impl_->buffered_ranges.end(), range_less_than));
#endif
}

}  // namespace media
}  // namespace shaka
