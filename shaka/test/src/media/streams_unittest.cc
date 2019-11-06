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

#include <gtest/gtest.h>
#include <math.h>

namespace shaka {
namespace media {

namespace {

std::shared_ptr<BaseFrame> MakeFrame(double start, double end,
                                     bool is_key_frame = true) {
  auto* ret = new BaseFrame(start, start, end - start, is_key_frame);
  return std::shared_ptr<BaseFrame>(ret);
}

using StreamType = StreamNew<BaseFrame, true>;

}  // namespace

TEST(StreamBaseTest, CreatesFirstRange) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(10, ranges[0].end);
}

TEST(StreamBaseTest, CreatesNewRangeAtStart) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(20, 30));

  // Should create a new range before the original.
  buffer.AddFrame(MakeFrame(0, 10));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(2u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(10, ranges[0].end);
  EXPECT_EQ(20, ranges[1].start);
  EXPECT_EQ(30, ranges[1].end);
}

TEST(StreamBaseTest, CreatesNewRangeAtEnd) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));

  // Should create a new range after the original.
  buffer.AddFrame(MakeFrame(20, 30));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(2u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(10, ranges[0].end);
  EXPECT_EQ(20, ranges[1].start);
  EXPECT_EQ(30, ranges[1].end);
}

TEST(StreamBaseTest, CreatesNewRangeInMiddle) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));
  buffer.AddFrame(MakeFrame(40, 50));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  // Should create a new range between the two existing ranges.
  buffer.AddFrame(MakeFrame(20, 30));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(3u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(10, ranges[0].end);
  EXPECT_EQ(20, ranges[1].start);
  EXPECT_EQ(30, ranges[1].end);
  EXPECT_EQ(40, ranges[2].start);
  EXPECT_EQ(50, ranges[2].end);
}

TEST(StreamBaseTest, AddsToEndOfExistingRange) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));

  // Should add to the exiting range.
  buffer.AddFrame(MakeFrame(10, 20));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(20, ranges[0].end);
}

TEST(StreamBaseTest, AddsToMiddleOfExistingRange) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));
  buffer.AddFrame(MakeFrame(10, 20));

  // Should insert the frame in between the existing two. The frames should be
  // in pts order, even though they are overlapping.
  buffer.AddFrame(MakeFrame(5, 10));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(20, ranges[0].end);
}

TEST(StreamBaseTest, AddsToBeginningOfExistingRange) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(10, 20));

  // Should add to the exiting range.
  buffer.AddFrame(MakeFrame(0, 10));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(20, ranges[0].end);
}

TEST(StreamBaseTest, StillAddsToExistingWithGap) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));

  // Should add to the exiting range.
  buffer.AddFrame(MakeFrame(10.01, 20));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(20, ranges[0].end);
}

TEST(StreamBaseTest, CombinesOverlappingRanges) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));
  buffer.AddFrame(MakeFrame(20, 30));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  // Should result in combining the two ranges.
  buffer.AddFrame(MakeFrame(10, 20));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(30, ranges[0].end);
}

TEST(StreamBaseTest, CombinesRangesWithSmallGap) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));
  buffer.AddFrame(MakeFrame(20, 30));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  // Should result in combining the two ranges.
  buffer.AddFrame(MakeFrame(10, 19.99));

  auto ranges = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, ranges.size());
  EXPECT_EQ(0, ranges[0].start);
  EXPECT_EQ(30, ranges[0].end);
}

TEST(StreamBaseTest, UsesPtsForBufferedRanges) {
  // This should use the PTS of the frames for buffered ranges, even when we
  // are sorted on DTS.  This means that the first frame in the range may not
  // define the time ranges for it.
  StreamType buffer;

  auto makeFrame = [](double dts, double pts) {
    return std::unique_ptr<BaseFrame>(new BaseFrame(pts, dts, 1, true));
  };

  // Range 1: DTS (0, 1, 2), PTS (1, 0, 2)
  buffer.AddFrame(makeFrame(0, 1));
  buffer.AddFrame(makeFrame(1, 0));
  buffer.AddFrame(makeFrame(2, 2));

  // Range 2: DTS (10, 11, 12), PTS (10, 12, 11)
  buffer.AddFrame(makeFrame(10, 10));
  buffer.AddFrame(makeFrame(11, 12));
  buffer.AddFrame(makeFrame(12, 11));

  auto buffered = buffer.GetBufferedRanges();
  ASSERT_EQ(2u, buffered.size());
  EXPECT_EQ(0, buffered[0].start);
  EXPECT_EQ(3, buffered[0].end);
  EXPECT_EQ(10, buffered[1].start);
  EXPECT_EQ(13, buffered[1].end);
}

TEST(StreamBaseTest, CountFramesBetween) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));
  buffer.AddFrame(MakeFrame(10, 20));
  buffer.AddFrame(MakeFrame(20, 30));
  buffer.AddFrame(MakeFrame(30, 40));
  //
  buffer.AddFrame(MakeFrame(100, 110));
  buffer.AddFrame(MakeFrame(110, 120));
  buffer.AddFrame(MakeFrame(120, 130));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  EXPECT_EQ(0, buffer.CountFramesBetween(0, 0));
  EXPECT_EQ(0, buffer.CountFramesBetween(0, 10));
  EXPECT_EQ(0, buffer.CountFramesBetween(5, 10));
  EXPECT_EQ(2, buffer.CountFramesBetween(0, 30));
  EXPECT_EQ(3, buffer.CountFramesBetween(0, 100));
  EXPECT_EQ(4, buffer.CountFramesBetween(0, 105));
  EXPECT_EQ(4, buffer.CountFramesBetween(0, 110));
  EXPECT_EQ(2, buffer.CountFramesBetween(5, 30));
  EXPECT_EQ(2, buffer.CountFramesBetween(100, 200));
}


TEST(StreamBaseTest, GetFrame_KeyFrameBefore_FindsFrameBefore) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));
  buffer.AddFrame(MakeFrame(10, 20, false));
  buffer.AddFrame(MakeFrame(20, 30, false));
  ASSERT_EQ(1u, buffer.GetBufferedRanges().size());

  const BaseFrame* frame =
      buffer.GetFrame(15, FrameLocation::KeyFrameBefore).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(0, frame->pts);
}

TEST(StreamBaseTest, GetFrame_KeyFrameBefore_FindsExactFrame) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));
  buffer.AddFrame(MakeFrame(10, 20));
  buffer.AddFrame(MakeFrame(20, 30));
  ASSERT_EQ(1u, buffer.GetBufferedRanges().size());

  const BaseFrame* frame =
      buffer.GetFrame(10, FrameLocation::KeyFrameBefore).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(10, frame->pts);
}

TEST(StreamBaseTest, GetFrame_KeyFrameBefore_WontReturnFutureFrames) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(10, 20));
  buffer.AddFrame(MakeFrame(20, 30));
  buffer.AddFrame(MakeFrame(30, 40));
  ASSERT_EQ(1u, buffer.GetBufferedRanges().size());

  const BaseFrame* frame =
      buffer.GetFrame(0, FrameLocation::KeyFrameBefore).get();
  EXPECT_EQ(nullptr, frame);
}


TEST(StreamBaseTest, GetFrame_After_GetsNext) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));
  buffer.AddFrame(MakeFrame(10, 20));

  const BaseFrame* frame = buffer.GetFrame(0, FrameLocation::After).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(10, frame->pts);
}

TEST(StreamBaseTest, GetFrame_After_GetsNextAcrossRanges) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 2));
  buffer.AddFrame(MakeFrame(2, 3));
  buffer.AddFrame(MakeFrame(10, 12));
  buffer.AddFrame(MakeFrame(12, 14));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  const BaseFrame* frame = buffer.GetFrame(2, FrameLocation::After).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(10, frame->pts);
}

TEST(StreamBaseTest, GetFrame_After_ReturnsNull) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));

  ASSERT_EQ(nullptr, buffer.GetFrame(0, FrameLocation::After).get());
  ASSERT_EQ(nullptr, buffer.GetFrame(4, FrameLocation::After).get());
  ASSERT_EQ(nullptr, buffer.GetFrame(10, FrameLocation::After).get());
  ASSERT_EQ(nullptr, buffer.GetFrame(12, FrameLocation::After).get());
}

TEST(StreamBaseTest, GetFrame_Near_NextFrame) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(10, 10));

  const BaseFrame* frame = buffer.GetFrame(0, FrameLocation::Near).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(10, frame->pts);
}

TEST(StreamBaseTest, GetFrame_Near_NextFrameBetweenRanges) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 0));
  buffer.AddFrame(MakeFrame(10, 10));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  const BaseFrame* frame = buffer.GetFrame(7, FrameLocation::Near).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(10, frame->pts);
}

TEST(StreamBaseTest, GetFrame_Near_PastTheEnd) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));
  buffer.AddFrame(MakeFrame(10, 10));

  const BaseFrame* frame = buffer.GetFrame(22, FrameLocation::Near).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(10, frame->pts);
}

TEST(StreamBaseTest, GetFrame_Near_InPastBetweenRanges) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 1));
  buffer.AddFrame(MakeFrame(1, 2));
  buffer.AddFrame(MakeFrame(10, 11));
  buffer.AddFrame(MakeFrame(11, 12));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  const BaseFrame* frame = buffer.GetFrame(3, FrameLocation::Near).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(1, frame->pts);
}

TEST(StreamBaseTest, GetFrame_Near_GetsNearest) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));
  buffer.AddFrame(MakeFrame(10.01, 10));
  ASSERT_EQ(1u, buffer.GetBufferedRanges().size());

  const BaseFrame* frame = buffer.GetFrame(10.001, FrameLocation::Near).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(0, frame->pts);

  frame = buffer.GetFrame(10.009, FrameLocation::Near).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(10.01, frame->pts);
}

TEST(StreamBaseTest, GetFrame_Near_NearestOverlaping) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 10));
  buffer.AddFrame(MakeFrame(9, 20));
  ASSERT_EQ(1u, buffer.GetBufferedRanges().size());

  // Even though we are closer to 9 ([1].pts) than 10 ([0].pts + [0].duration),
  // we should pick [0] since we are in the frame.
  const BaseFrame* frame = buffer.GetFrame(8.9, FrameLocation::Near).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(0, frame->pts);
}

TEST(StreamBaseTest, GetFrame_Near_ReturnsNull) {
  // Since it returns the nearest frame always, the only case it returns null is
  // when there are no frames.
  StreamType buffer;
  const BaseFrame* frame = buffer.GetFrame(0, FrameLocation::Near).get();
  ASSERT_EQ(nullptr, frame);
}


TEST(StreamBaseTest, Remove_RemovesWholeRange) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 1));
  buffer.AddFrame(MakeFrame(1, 2));
  buffer.AddFrame(MakeFrame(2, 3));
  //
  buffer.AddFrame(MakeFrame(6, 7));
  buffer.AddFrame(MakeFrame(7, 8));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  buffer.Remove(6, 8);

  EXPECT_EQ(1u, buffer.GetBufferedRanges().size());
  EXPECT_EQ(nullptr, buffer.GetFrame(3, FrameLocation::After).get());
}

TEST(StreamBaseTest, Remove_SplitsRanges) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 1));
  buffer.AddFrame(MakeFrame(1, 2));
  buffer.AddFrame(MakeFrame(2, 3));
  buffer.AddFrame(MakeFrame(3, 4));
  buffer.AddFrame(MakeFrame(4, 5));
  ASSERT_EQ(1u, buffer.GetBufferedRanges().size());

  buffer.Remove(2, 4);

  auto buffered = buffer.GetBufferedRanges();
  ASSERT_EQ(2u, buffered.size());
  EXPECT_EQ(0, buffered[0].start);
  EXPECT_EQ(2, buffered[0].end);
  EXPECT_EQ(4, buffered[1].start);
  EXPECT_EQ(5, buffered[1].end);

  const BaseFrame* frame = buffer.GetFrame(1, FrameLocation::After).get();
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(4, frame->pts);
}

TEST(StreamBaseTest, Remove_RemovesPartOfRange) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 1));
  buffer.AddFrame(MakeFrame(1, 2));
  buffer.AddFrame(MakeFrame(2, 3));
  buffer.AddFrame(MakeFrame(3, 4));
  buffer.AddFrame(MakeFrame(4, 5));
  ASSERT_EQ(1u, buffer.GetBufferedRanges().size());

  buffer.Remove(3, 5);

  auto buffered = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, buffered.size());
  EXPECT_EQ(0, buffered[0].start);
  EXPECT_EQ(3, buffered[0].end);

  EXPECT_EQ(nullptr, buffer.GetFrame(2, FrameLocation::After).get());
}

TEST(StreamBaseTest, Remove_RemovesMultipleRanges) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 1));
  buffer.AddFrame(MakeFrame(1, 2));
  buffer.AddFrame(MakeFrame(2, 3));
  //
  buffer.AddFrame(MakeFrame(5, 6));
  buffer.AddFrame(MakeFrame(6, 7));
  //
  buffer.AddFrame(MakeFrame(10, 11));
  buffer.AddFrame(MakeFrame(11, 12));
  //
  buffer.AddFrame(MakeFrame(15, 16));
  buffer.AddFrame(MakeFrame(16, 17));
  buffer.AddFrame(MakeFrame(17, 18));
  ASSERT_EQ(4u, buffer.GetBufferedRanges().size());

  buffer.Remove(0, 7);

  auto buffered = buffer.GetBufferedRanges();
  ASSERT_EQ(2u, buffered.size());
  EXPECT_EQ(10, buffered[0].start);
  EXPECT_EQ(12, buffered[0].end);
  EXPECT_EQ(15, buffered[1].start);
  EXPECT_EQ(18, buffered[1].end);
}

TEST(StreamBaseTest, Remove_RemovesAllRanges) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 1));
  buffer.AddFrame(MakeFrame(1, 2));
  buffer.AddFrame(MakeFrame(2, 3));
  //
  buffer.AddFrame(MakeFrame(5, 6));
  buffer.AddFrame(MakeFrame(6, 7));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  buffer.Remove(0, 7);

  EXPECT_EQ(0u, buffer.GetBufferedRanges().size());
}

TEST(StreamBaseTest, Remove_RemovesNothing) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 1));
  buffer.AddFrame(MakeFrame(1, 2));
  buffer.AddFrame(MakeFrame(2, 3));
  //
  buffer.AddFrame(MakeFrame(5, 6));
  buffer.AddFrame(MakeFrame(6, 7));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  buffer.Remove(10, 20);

  auto buffered = buffer.GetBufferedRanges();
  ASSERT_EQ(2u, buffered.size());
  EXPECT_EQ(0, buffered[0].start);
  EXPECT_EQ(3, buffered[0].end);
  EXPECT_EQ(5, buffered[1].start);
  EXPECT_EQ(7, buffered[1].end);
}

TEST(StreamBaseTest, Remove_SupportsInfinity) {
  StreamType buffer;
  buffer.AddFrame(MakeFrame(2, 3));
  buffer.AddFrame(MakeFrame(3, 4));
  //
  buffer.AddFrame(MakeFrame(6, 7));
  buffer.AddFrame(MakeFrame(7, 8));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  buffer.Remove(0, HUGE_VAL /* Infinity */);

  EXPECT_EQ(0u, buffer.GetBufferedRanges().size());
}

TEST(StreamBaseTest, Remove_RemovesUntilKeyframe) {
  // When removing frames, it should remove frames past the given stop until the
  // next keyframe; see step 3.4 of the "Coded Frame Removal Algorithm" in MSE:
  // https://w3c.github.io/media-source/#sourcebuffer-coded-frame-removal
  StreamType buffer;
  buffer.AddFrame(MakeFrame(0, 1));
  buffer.AddFrame(MakeFrame(1, 2));
  buffer.AddFrame(MakeFrame(2, 3, false));
  buffer.AddFrame(MakeFrame(3, 4, false));
  buffer.AddFrame(MakeFrame(6, 7));
  buffer.AddFrame(MakeFrame(7, 8));
  ASSERT_EQ(2u, buffer.GetBufferedRanges().size());

  buffer.Remove(0, 2);  // Should actually remove [0, 4].

  auto buffered = buffer.GetBufferedRanges();
  ASSERT_EQ(1u, buffered.size());
  EXPECT_EQ(6, buffered[0].start);
  EXPECT_EQ(8, buffered[0].end);
}

}  // namespace media
}  // namespace shaka
