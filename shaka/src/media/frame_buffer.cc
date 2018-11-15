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

#include "src/media/frame_buffer.h"

#include <glog/logging.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <unordered_set>
#include <utility>

namespace shaka {
namespace media {

namespace {

// These exist to reduce the number of if checks for ordering.  This is done
// by using a single if at the beginning of the method and using a function
// pointer to one of these methods.

template <bool OrderByDts = true>
double GetTime(const std::unique_ptr<const BaseFrame>& a) {
  return a->dts;
}
template <>
double GetTime<false>(const std::unique_ptr<const BaseFrame>& a) {
  return a->pts;
}

#ifndef NDEBUG
template <bool OrderByDts>
bool FrameLessThan(const std::unique_ptr<const BaseFrame>& a,
                   const std::unique_ptr<const BaseFrame>& b) {
  return GetTime<OrderByDts>(a) < GetTime<OrderByDts>(b);
}
#endif

template <bool OrderByDts>
bool FrameExtendsPast(const std::unique_ptr<const BaseFrame>& a,
                      const std::unique_ptr<const BaseFrame>& b) {
  return GetTime<OrderByDts>(a) + a->duration + FrameBuffer::kMaxGapSize >=
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
std::list<std::unique_ptr<const BaseFrame>>::iterator FrameLowerBound(
    const std::list<std::unique_ptr<const BaseFrame>>& list, double time) {
  auto& mutable_list =
      const_cast<std::list<std::unique_ptr<const BaseFrame>>&>(list);  // NOLINT
  if (list.empty())
    return mutable_list.end();

  if (time - GetTime<OrderByDts>(list.front()) <
      GetTime<OrderByDts>(list.back()) - time) {
    // Closer to the front, find the first element that is not less than |time|.
    auto notLessThan = [&time](const std::unique_ptr<const BaseFrame>& other) {
      return GetTime<OrderByDts>(other) >= time;
    };
    return std::find_if(mutable_list.begin(), mutable_list.end(), notLessThan);
  }

  // Closer to the end, first use a reverse search to find the first (in
  // reverse order) that is less than |time|.
  auto isLessThan = [&time](const std::unique_ptr<const BaseFrame>& other) {
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

}  // namespace

FrameBuffer::FrameBuffer(bool order_by_dts)
    : mutex_("FrameBuffer"), order_by_dts_(order_by_dts) {}

FrameBuffer::~FrameBuffer() {}

size_t FrameBuffer::EstimateSize() const {
  std::unique_lock<Mutex> lock(mutex_);

  size_t size = 0;
  for (auto& range : buffered_ranges_) {
    for (auto& frame : range.frames)
      size += frame->EstimateSize();
  }
  return size;
}

void FrameBuffer::AppendFrame(std::unique_ptr<const BaseFrame> frame) {
  std::unique_lock<Mutex> lock(mutex_);
  DCHECK(frame);

  auto extendsPast =
      order_by_dts_ ? &FrameExtendsPast<true> : &FrameExtendsPast<false>;
  auto lowerBound =
      order_by_dts_ ? &FrameLowerBound<true> : &FrameLowerBound<false>;
  auto getTime = order_by_dts_ ? &GetTime<true> : &GetTime<false>;

  // Find the first buffered range that ends after |frame|.
  auto range_it = std::find_if(buffered_ranges_.begin(), buffered_ranges_.end(),
                               [&](const Range& range) {
                                 return extendsPast(range.frames.back(), frame);
                               });

  if (range_it == buffered_ranges_.end()) {
    // |frame| was after every existing range, create a new one.
    buffered_ranges_.emplace_back();
    buffered_ranges_.back().start_pts = frame->pts;
    buffered_ranges_.back().end_pts = frame->pts + frame->duration;
    buffered_ranges_.back().frames.emplace_back(std::move(frame));
  } else if (!extendsPast(frame, range_it->frames.front())) {
    // |frame| is before this range, so it starts a new range before this one.
    auto it = buffered_ranges_.emplace(range_it);
    it->start_pts = frame->pts;
    it->end_pts = frame->pts + frame->duration;
    it->frames.emplace_back(std::move(frame));
  } else {
    // |frame| is inside the current range.
    auto frame_it = lowerBound(range_it->frames, getTime(frame));
    range_it->start_pts = std::min(range_it->start_pts, frame->pts);
    range_it->end_pts =
        std::max(range_it->end_pts, frame->pts + frame->duration);
    if (frame_it != range_it->frames.end() &&
        getTime(*frame_it) == getTime(frame)) {
      used_frames_.WaitToDeleteFrames({frame_it->get()});
      swap(*frame_it, frame);
    } else {
      range_it->frames.insert(frame_it, std::move(frame));
    }
  }

  // If the frame closed a gap, then merge the buffered ranges.
  DCHECK_NE(0u, buffered_ranges_.size());
  for (auto it = std::next(buffered_ranges_.begin());
       it != buffered_ranges_.end();) {
    auto prev = std::prev(it);
    if (extendsPast(prev->frames.back(), it->frames.front())) {
      // Move all elements from |it->frames| to the end of |prev->frames|.
      // Since both lists are sorted and |prev < it|, this will remain sorted.
      prev->frames.splice(prev->frames.end(), it->frames);
      prev->start_pts = std::min(prev->start_pts, it->start_pts);
      prev->end_pts = std::max(prev->end_pts, it->end_pts);
      it = buffered_ranges_.erase(it);
    } else {
      it++;
    }
  }

  AssertRangesSorted();
}

BufferedRanges FrameBuffer::GetBufferedRanges() const {
  std::unique_lock<Mutex> lock(mutex_);
  AssertRangesSorted();

  BufferedRanges ret;
  ret.reserve(buffered_ranges_.size());
  for (const Range& range : buffered_ranges_) {
    ret.emplace_back(range.start_pts, range.end_pts);
  }
  return ret;
}

int FrameBuffer::FramesBetween(double start_time, double end_time) const {
  std::unique_lock<Mutex> lock(mutex_);
  AssertRangesSorted();

  auto getTime = order_by_dts_ ? &GetTime<true> : &GetTime<false>;
  auto lowerBound =
      order_by_dts_ ? &FrameLowerBound<true> : &FrameLowerBound<false>;

  // Find the first buffered range that includes or is after |start_time|.
  auto range_it =
      std::find_if(buffered_ranges_.begin(), buffered_ranges_.end(),
                   [&](const Range& range) {
                     return getTime(range.frames.back()) >= start_time;
                   });

  int num_frames = 0;
  for (; range_it != buffered_ranges_.end(); range_it++) {
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

LockedFrameList::Guard FrameBuffer::GetFrameNear(double time) const {
  std::unique_lock<Mutex> lock(mutex_);
  return used_frames_.GuardFrame(GetFrameNear(time, /* allow_before */ true));
}

LockedFrameList::Guard FrameBuffer::GetFrameAfter(double time) const {
  std::unique_lock<Mutex> lock(mutex_);
  return used_frames_.GuardFrame(GetFrameNear(time, /* allow_before */ false));
}

LockedFrameList::Guard FrameBuffer::GetKeyFrameBefore(double time) const {
  std::unique_lock<Mutex> lock(mutex_);
  AssertRangesSorted();

  auto getTime = order_by_dts_ ? &GetTime<true> : &GetTime<false>;
  auto lowerBound =
      order_by_dts_ ? &FrameLowerBound<true> : &FrameLowerBound<false>;

  // Find the first buffered range that includes or is after |time|.
  auto range_it = std::find_if(
      buffered_ranges_.begin(), buffered_ranges_.end(),
      [&](const Range& range) { return getTime(range.frames.back()) >= time; });

  if (range_it != buffered_ranges_.end()) {
    // |it| points to the frame that starts at or greater than |time|.
    auto it = lowerBound(range_it->frames, time);
    DCHECK(it != range_it->frames.end());

    if (getTime(*it) > time) {
      if (it == range_it->frames.begin())
        return LockedFrameList::Guard();
      it--;
    }

    for (; it != range_it->frames.begin(); it--) {
      if ((*it)->is_key_frame)
        return used_frames_.GuardFrame(it->get());
    }

    CHECK((*it)->is_key_frame);
    return used_frames_.GuardFrame(it->get());
  }

  return LockedFrameList::Guard();
}

void FrameBuffer::Remove(double start, double end) {
  // Note that remove always uses PTS, even when sorting using DTS.  This is
  // intended to work like the MSE definition.

  std::unique_lock<Mutex> lock(mutex_);
  bool is_removing = false;
  for (auto it = buffered_ranges_.begin(); it != buffered_ranges_.end();) {
    // These represent the range of frames within this buffer to delete.
    auto frame_del_start = is_removing ? it->frames.begin() : it->frames.end();
    auto frame_del_end = it->frames.end();

    std::unordered_set<const BaseFrame*> frames_to_remove;
    for (auto frame = it->frames.begin(); frame != it->frames.end(); frame++) {
      if (!is_removing) {
        // Only start deleting frames whose start time is in the range.
        if ((*frame)->pts >= start && (*frame)->pts < end) {
          is_removing = true;
          frame_del_start = frame;
          frames_to_remove.insert(frame->get());
        }
      } else if ((*frame)->pts >= end && (*frame)->is_key_frame) {
        // The MSE spec says to remove up to the next key frame.
        frame_del_end = frame;
        is_removing = false;
        break;
      } else {
        frames_to_remove.insert(frame->get());
      }
    }

    // We don't release |mutex_| while waiting.  Any threads using frames should
    // not make calls into this FrameBuffer (though they can to other buffers).
    used_frames_.WaitToDeleteFrames(frames_to_remove);

    if (frame_del_start != it->frames.begin() &&
        frame_del_start != it->frames.end() &&
        frame_del_end != it->frames.end()) {
      // We deleted a partial range, so we need to split the buffered range.
      auto new_it = buffered_ranges_.emplace(it);  // new_it + 1 == it

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
        it = buffered_ranges_.erase(it);
      } else {
        UpdatePtsRanges(it);
        it++;
      }
    }
  }

  AssertRangesSorted();
}

const BaseFrame* FrameBuffer::GetFrameNear(double time,
                                           bool allow_before) const {
  AssertRangesSorted();

  auto getTime = order_by_dts_ ? &GetTime<true> : &GetTime<false>;
  auto lowerBound =
      order_by_dts_ ? &FrameLowerBound<true> : &FrameLowerBound<false>;

  // Find the first buffered range that includes or is after |time|.
  auto it = std::find_if(
      buffered_ranges_.begin(), buffered_ranges_.end(),
      [&](const Range& range) { return getTime(range.frames.back()) >= time; });

  if (it != buffered_ranges_.end()) {
    // |frame_it| points to the frame that starts at or greater than |time|.
    auto frame_it = lowerBound(it->frames, time);
    DCHECK(frame_it != it->frames.end());

    // Find the frame after |time|.
    auto next_it = it->frames.end();
    bool has_next = true;
    if (getTime(*frame_it) > time)
      next_it = frame_it;
    else if (std::next(frame_it) != it->frames.end())
      next_it = std::next(frame_it);
    else if (std::next(it) != buffered_ranges_.end())
      next_it = std::next(it)->frames.begin();
    else
      has_next = false;

    // Find the frame before |time|.  This is only for GetFrameNear, so allow
    // returning frame that equals |time|.
    if (allow_before) {
      auto prev_it = it->frames.end();
      bool has_prev = true;
      if (getTime(*frame_it) <= time)
        prev_it = frame_it;
      else if (frame_it != it->frames.begin())
        prev_it = std::prev(frame_it);
      else if (it != buffered_ranges_.begin())
        prev_it = std::prev(std::prev(it)->frames.end());
      else
        has_prev = false;

      if (has_prev) {
        const double prev_dt = time - getTime(*prev_it) - (*prev_it)->duration;
        if (!has_next || getTime(*next_it) - time > prev_dt)
          return prev_it->get();
      }
    }

    return has_next ? next_it->get() : nullptr;
  }

  if (allow_before && !buffered_ranges_.empty())
    return buffered_ranges_.back().frames.back().get();

  return nullptr;
}

void FrameBuffer::AssertRangesSorted() const {
#ifndef NDEBUG
  auto frame_less_than =
      order_by_dts_ ? &FrameLessThan<true> : &FrameLessThan<false>;
  auto range_is_valid = [&](const FrameBuffer::Range& range) {
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
  auto range_less_than = [&](const FrameBuffer::Range& first,
                             const FrameBuffer::Range& second) {
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

  CHECK(std::all_of(buffered_ranges_.begin(), buffered_ranges_.end(),
                    range_is_valid));
  CHECK(std::is_sorted(buffered_ranges_.begin(), buffered_ranges_.end(),
                       range_less_than));
#endif
}

FrameBuffer::Range::Range() : start_pts(HUGE_VAL), end_pts(-HUGE_VAL) {}
FrameBuffer::Range::~Range() {}

}  // namespace media
}  // namespace shaka
