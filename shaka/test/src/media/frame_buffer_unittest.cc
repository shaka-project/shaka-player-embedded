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

#include <math.h>

#include <gtest/gtest.h>

namespace shaka {
namespace media {

namespace {

constexpr const bool kPtsOrder = false;
constexpr const bool kDtsOrder = true;

std::unique_ptr<BaseFrame> MakeFrame(double start, double end,
                                     bool is_key_frame = true) {
  BaseFrame* ret = new BaseFrame(start, start, end - start, is_key_frame);
  return std::unique_ptr<BaseFrame>(ret);
}

}  // namespace

TEST(FrameBufferTest, CreatesFirstRange) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(10, ranges[0].end);
}

TEST(FrameBufferTest, CreatesNewRangeAtStart) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(20, 30));

  // Should create a new range before the original.
  buffer.AppendFrame(MakeFrame(0, 10));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(2u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(10, ranges[0].end);
  EXPECT_EQ(20, ranges[1].start);
  EXPECT_EQ(30, ranges[1].end);
}

TEST(FrameBufferTest, CreatesNewRangeAtEnd) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));

  // Should create a new range after the original.
  buffer.AppendFrame(MakeFrame(20, 30));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(2u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(10, ranges[0].end);
  EXPECT_EQ(20, ranges[1].start);
  EXPECT_EQ(30, ranges[1].end);
}

TEST(FrameBufferTest, CreatesNewRangeInMiddle) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));
  buffer.AppendFrame(MakeFrame(40, 50));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  // Should create a new range between the two existing ranges.
  buffer.AppendFrame(MakeFrame(20, 30));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(3u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(10, ranges[0].end);
  EXPECT_EQ(20, ranges[1].start);
  EXPECT_EQ(30, ranges[1].end);
  EXPECT_EQ(40, ranges[2].start);
  EXPECT_EQ(50, ranges[2].end);
}

TEST(FrameBufferTest, AddsToEndOfExistingRange) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));

  // Should add to the exiting range.
  buffer.AppendFrame(MakeFrame(10, 20));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(20, ranges[0].end);
}

TEST(FrameBufferTest, AddsToMiddleOfExistingRange) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));
  buffer.AppendFrame(MakeFrame(10, 20));

  // Should insert the frame in between the existing two. The frames should be
  // in pts order, even though they are overlapping.
  buffer.AppendFrame(MakeFrame(5, 10));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(20, ranges[0].end);
}

TEST(FrameBufferTest, AddsToBeginningOfExistingRange) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(10, 20));

  // Should add to the exiting range.
  buffer.AppendFrame(MakeFrame(0, 10));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(20, ranges[0].end);
}

TEST(FrameBufferTest, StillAddsToExistingWithGap) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));

  // Should add to the exiting range.
  buffer.AppendFrame(MakeFrame(10.01, 20));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(20, ranges[0].end);
}

TEST(FrameBufferTest, CombinesOverlappingRanges) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));
  buffer.AppendFrame(MakeFrame(20, 30));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  // Should result in combining the two ranges.
  buffer.AppendFrame(MakeFrame(10, 20));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(30, ranges[0].end);
}

TEST(FrameBufferTest, CombinesRangesWithSmallGap) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));
  buffer.AppendFrame(MakeFrame(20, 30));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  // Should result in combining the two ranges.
  buffer.AppendFrame(MakeFrame(10, 19.99));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(30, ranges[0].end);
}

TEST(FrameBufferTest, UsesPtsForBufferedRanges) {
  // This should use the PTS of the frames for buffered ranges, even when we
  // are sorted on DTS.  This means that the first frame in the range may not
  // define the time ranges for it.
  FrameBuffer buffer(kDtsOrder);

  auto makeFrame = [](double dts, double pts) {
    return std::unique_ptr<BaseFrame>(new BaseFrame(pts, dts, 1, true));
  };

  // Range 1: DTS (0, 1, 2), PTS (1, 0, 2)
  buffer.AppendFrame(makeFrame(0, 1));
  buffer.AppendFrame(makeFrame(1, 0));
  buffer.AppendFrame(makeFrame(2, 2));

  // Range 2: DTS (10, 11, 12), PTS (10, 12, 11)
  buffer.AppendFrame(makeFrame(10, 10));
  buffer.AppendFrame(makeFrame(11, 12));
  buffer.AppendFrame(makeFrame(12, 11));

  auto buffered = buffer.GetBufferedRanges();
  ASSERT_EQ(2u, buffered.size());
  EXPECT_EQ(0, buffered[0].start);
  EXPECT_EQ(3, buffered[0].end);
  EXPECT_EQ(10, buffered[1].start);
  EXPECT_EQ(13, buffered[1].end);
}

TEST(FrameBufferTest, FramesBetween) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));
  buffer.AppendFrame(MakeFrame(10, 20));
  buffer.AppendFrame(MakeFrame(20, 30));
  buffer.AppendFrame(MakeFrame(30, 40));
  //
  buffer.AppendFrame(MakeFrame(100, 110));
  buffer.AppendFrame(MakeFrame(110, 120));
  buffer.AppendFrame(MakeFrame(120, 130));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  EXPECT_EQ(0, buffer.FramesBetween(0, 0));
  EXPECT_EQ(0, buffer.FramesBetween(0, 10));
  EXPECT_EQ(0, buffer.FramesBetween(5, 10));
  EXPECT_EQ(2, buffer.FramesBetween(0, 30));
  EXPECT_EQ(3, buffer.FramesBetween(0, 100));
  EXPECT_EQ(4, buffer.FramesBetween(0, 105));
  EXPECT_EQ(4, buffer.FramesBetween(0, 110));
  EXPECT_EQ(2, buffer.FramesBetween(5, 30));
  EXPECT_EQ(2, buffer.FramesBetween(100, 200));
}


TEST(FrameBufferTest, GetKeyFrameBefore_FindsFrameBefore) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));
  buffer.AppendFrame(MakeFrame(10, 20, false));
  buffer.AppendFrame(MakeFrame(20, 30, false));
  ASSERT_EQ(1u, buffer.GetBufferedRanges().size());

  const BaseFrame* frame = buffer.GetKeyFrameBefore(15).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(0, frame->pts);
}

TEST(FrameBufferTest, GetKeyFrameBefore_FindsExactFrame) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));
  buffer.AppendFrame(MakeFrame(10, 20));
  buffer.AppendFrame(MakeFrame(20, 30));
  ASSERT_EQ(1u, buffer.GetBufferedRanges().size());

  const BaseFrame* frame = buffer.GetKeyFrameBefore(10).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(10, frame->pts);
}

TEST(FrameBufferTest, GetKeyFrameBefore_WontReturnFutureFrames) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(10, 20));
  buffer.AppendFrame(MakeFrame(20, 30));
  buffer.AppendFrame(MakeFrame(30, 40));
  ASSERT_EQ(1u, buffer.GetBufferedRanges().size());

  const BaseFrame* frame = buffer.GetKeyFrameBefore(0).get();
  EXPECT_EQ(nullptr, frame);
}


TEST(FrameBufferTest, GetFrameAfter_GetsNext) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));
  buffer.AppendFrame(MakeFrame(10, 20));

  const BaseFrame* frame = buffer.GetFrameAfter(0).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(10, frame->pts);
}

TEST(FrameBufferTest, GetFrameAfter_GetsNextAcrossRanges) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 2));
  buffer.AppendFrame(MakeFrame(2, 3));
  buffer.AppendFrame(MakeFrame(10, 12));
  buffer.AppendFrame(MakeFrame(12, 14));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  const BaseFrame* frame = buffer.GetFrameAfter(2).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(10, frame->pts);
}

TEST(FrameBufferTest, GetFrameAfter_ReturnsNull) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));

  ASSERT_EQ(nullptr, buffer.GetFrameAfter(0).get());
  ASSERT_EQ(nullptr, buffer.GetFrameAfter(4).get());
  ASSERT_EQ(nullptr, buffer.GetFrameAfter(10).get());
  ASSERT_EQ(nullptr, buffer.GetFrameAfter(12).get());
}

TEST(FrameBufferTest, GetFrameNear_NextFrame) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(10, 10));

  const BaseFrame* frame = buffer.GetFrameNear(0).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(10, frame->pts);
}

TEST(FrameBufferTest, GetFrameNear_NextFrameBetweenRanges) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 0));
  buffer.AppendFrame(MakeFrame(10, 10));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  const BaseFrame* frame = buffer.GetFrameNear(7).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(10, frame->pts);
}

TEST(FrameBufferTest, GetFrameNear_PastTheEnd) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));
  buffer.AppendFrame(MakeFrame(10, 10));

  const BaseFrame* frame = buffer.GetFrameNear(12).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(10, frame->pts);
}

TEST(FrameBufferTest, GetFrameNear_InPastBetweenRanges) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 1));
  buffer.AppendFrame(MakeFrame(1, 2));
  buffer.AppendFrame(MakeFrame(10, 11));
  buffer.AppendFrame(MakeFrame(11, 12));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  const BaseFrame* frame = buffer.GetFrameNear(3).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(1, frame->pts);
}

TEST(FrameBufferTest, GetFrameNear_GetsNearest) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 10));
  buffer.AppendFrame(MakeFrame(10.01, 10));
  ASSERT_EQ(1u, buffer.GetBufferedRanges().size());

  const BaseFrame* frame = buffer.GetFrameNear(10.001).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(0, frame->pts);

  frame = buffer.GetFrameNear(10.009).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(10.01, frame->pts);
}

TEST(FrameBufferTest, GetFrameNear_ReturnsNull) {
  // Since it returns the nearest frame always, the only case it returns null is
  // when there are no frames.
  FrameBuffer buffer(kPtsOrder);
  const BaseFrame* frame = buffer.GetFrameNear(0).get();
  ASSERT_EQ(nullptr, frame);
}


TEST(FrameBufferTest, Remove_RemovesWholeRange) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 1));
  buffer.AppendFrame(MakeFrame(1, 2));
  buffer.AppendFrame(MakeFrame(2, 3));
  //
  buffer.AppendFrame(MakeFrame(6, 7));
  buffer.AppendFrame(MakeFrame(7, 8));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  buffer.Remove(6, 8);

  EXPECT_EQ(1u, buffer.GetBufferedRanges().size());
  EXPECT_EQ(nullptr, buffer.GetFrameAfter(3).get());
}

TEST(FrameBufferTest, Remove_SplitsRanges) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 1));
  buffer.AppendFrame(MakeFrame(1, 2));
  buffer.AppendFrame(MakeFrame(2, 3));
  buffer.AppendFrame(MakeFrame(3, 4));
  buffer.AppendFrame(MakeFrame(4, 5));
  ASSERT_EQ(1u, buffer.GetBufferedRanges().size());

  buffer.Remove(2, 4);

  auto buffered = buffer.GetBufferedRanges();
  ASSERT_EQ(2u, buffered.size());
  EXPECT_EQ(0, buffered[0].start);
  EXPECT_EQ(2, buffered[0].end);
  EXPECT_EQ(4, buffered[1].start);
  EXPECT_EQ(5, buffered[1].end);

  const BaseFrame* frame = buffer.GetFrameAfter(1).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(4, frame->pts);
}

TEST(FrameBufferTest, Remove_RemovesPartOfRange) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 1));
  buffer.AppendFrame(MakeFrame(1, 2));
  buffer.AppendFrame(MakeFrame(2, 3));
  buffer.AppendFrame(MakeFrame(3, 4));
  buffer.AppendFrame(MakeFrame(4, 5));
  ASSERT_EQ(1u, buffer.GetBufferedRanges().size());

  buffer.Remove(3, 5);

  auto buffered = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, buffered.size());
  EXPECT_EQ(0, buffered[0].start);
  EXPECT_EQ(3, buffered[0].end);

  EXPECT_EQ(nullptr, buffer.GetFrameAfter(2).get());
}

TEST(FrameBufferTest, Remove_RemovesMultipleRanges) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 1));
  buffer.AppendFrame(MakeFrame(1, 2));
  buffer.AppendFrame(MakeFrame(2, 3));
  //
  buffer.AppendFrame(MakeFrame(5, 6));
  buffer.AppendFrame(MakeFrame(6, 7));
  //
  buffer.AppendFrame(MakeFrame(10, 11));
  buffer.AppendFrame(MakeFrame(11, 12));
  //
  buffer.AppendFrame(MakeFrame(15, 16));
  buffer.AppendFrame(MakeFrame(16, 17));
  buffer.AppendFrame(MakeFrame(17, 18));
  ASSERT_EQ(4u, buffer.GetBufferedRanges().size());

  buffer.Remove(0, 7);

  auto buffered = buffer.GetBufferedRanges();
  ASSERT_EQ(2u, buffered.size());
  EXPECT_EQ(10, buffered[0].start);
  EXPECT_EQ(12, buffered[0].end);
  EXPECT_EQ(15, buffered[1].start);
  EXPECT_EQ(18, buffered[1].end);
}

TEST(FrameBufferTest, Remove_RemovesAllRanges) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 1));
  buffer.AppendFrame(MakeFrame(1, 2));
  buffer.AppendFrame(MakeFrame(2, 3));
  //
  buffer.AppendFrame(MakeFrame(5, 6));
  buffer.AppendFrame(MakeFrame(6, 7));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  buffer.Remove(0, 7);

  EXPECT_EQ(0u, buffer.GetBufferedRanges().size());
}

TEST(FrameBufferTest, Remove_RemovesNothing) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 1));
  buffer.AppendFrame(MakeFrame(1, 2));
  buffer.AppendFrame(MakeFrame(2, 3));
  //
  buffer.AppendFrame(MakeFrame(5, 6));
  buffer.AppendFrame(MakeFrame(6, 7));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  buffer.Remove(10, 20);

  auto buffered = buffer.GetBufferedRanges();
  ASSERT_EQ(2u, buffered.size());
  EXPECT_EQ(0, buffered[0].start);
  EXPECT_EQ(3, buffered[0].end);
  EXPECT_EQ(5, buffered[1].start);
  EXPECT_EQ(7, buffered[1].end);
}

TEST(FrameBufferTest, Remove_SupportsInfinity) {
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(2, 3));
  buffer.AppendFrame(MakeFrame(3, 4));
  //
  buffer.AppendFrame(MakeFrame(6, 7));
  buffer.AppendFrame(MakeFrame(7, 8));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  buffer.Remove(0, HUGE_VAL /* Infinity */);

  EXPECT_EQ(0u, buffer.GetBufferedRanges().size());
}

TEST(FrameBufferTest, Remove_RemovesUntilKeyframe) {
  // When removing frames, it should remove frames past the given stop until the
  // next keyframe; see step 3.4 of the "Coded Frame Removal Algorithm" in MSE:
  // https://w3c.github.io/media-source/#sourcebuffer-coded-frame-removal
  FrameBuffer buffer(kPtsOrder);
  buffer.AppendFrame(MakeFrame(0, 1));
  buffer.AppendFrame(MakeFrame(1, 2));
  buffer.AppendFrame(MakeFrame(2, 3, false));
  buffer.AppendFrame(MakeFrame(3, 4, false));
  buffer.AppendFrame(MakeFrame(6, 7));
  buffer.AppendFrame(MakeFrame(7, 8));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  buffer.Remove(0, 2);  // Should actually remove [0, 4].

  auto buffered = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, buffered.size());
  EXPECT_EQ(6, buffered[0].start);
  EXPECT_EQ(8, buffered[0].end);
}

}  // namespace media
}  // namespace shaka
