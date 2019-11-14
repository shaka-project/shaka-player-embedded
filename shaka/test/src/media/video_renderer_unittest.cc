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

#include "src/media/video_renderer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shaka/media/frames.h"
#include "shaka/media/streams.h"

namespace shaka {
namespace media {

using testing::_;
using testing::InSequence;
using testing::MockFunction;
using testing::Return;

namespace {

std::shared_ptr<DecodedFrame> MakeFrame(double start) {
  auto* ret = new DecodedFrame(start, start, 0.01, PixelFormat::RGB24, 0, 0, 0,
                               0, 0, {}, {});
  return std::shared_ptr<DecodedFrame>(ret);
}

constexpr const double kMinDelay = 1.0 / 120;

}  // namespace

TEST(VideoRendererTest, WorksWithNoNextFrame) {
  DecodedStream stream;
  auto frame = MakeFrame(0.0);
  stream.AddFrame(frame);

  MockFunction<double()> get_time;
  ON_CALL(get_time, Call()).WillByDefault(Return(0));

  VideoRenderer renderer(std::bind(&MockFunction<double()>::Call, &get_time),
                         &stream);

  int dropped_frame_count = 0;
  bool is_new_frame = false;
  double delay = 0;
  auto ret = renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(ret, frame);
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_TRUE(is_new_frame);
  EXPECT_EQ(delay, kMinDelay);
}

TEST(VideoRendererTest, WorksWithNoFrames) {
  DecodedStream stream;

  MockFunction<double()> get_time;
  ON_CALL(get_time, Call()).WillByDefault(Return(0));

  VideoRenderer renderer(std::bind(&MockFunction<double()>::Call, &get_time),
                         &stream);

  int dropped_frame_count = 0;
  bool is_new_frame = false;
  double delay = 0;
  auto ret = renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_FALSE(ret);
}

TEST(VideoRendererTest, DrawsFrameInPast) {
  DecodedStream stream;
  auto frame = MakeFrame(0.0);
  stream.AddFrame(frame);

  MockFunction<double()> get_time;
  ON_CALL(get_time, Call()).WillByDefault(Return(4));

  VideoRenderer renderer(std::bind(&MockFunction<double()>::Call, &get_time),
                         &stream);

  int dropped_frame_count = 0;
  bool is_new_frame = false;
  double delay = 0;
  auto ret = renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(ret, frame);
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, kMinDelay);
}

TEST(VideoRendererTest, WillDropFrames) {
  DecodedStream stream;
  stream.AddFrame(MakeFrame(0.00));
  stream.AddFrame(MakeFrame(0.01));
  stream.AddFrame(MakeFrame(0.02));
  stream.AddFrame(MakeFrame(0.03));
  stream.AddFrame(MakeFrame(0.04));

  MockFunction<double()> get_time;
  MockFunction<void()> step;

  {
    InSequence seq;
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0));
    EXPECT_CALL(step, Call()).Times(1);
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0.03));
  }

  VideoRenderer renderer(std::bind(&MockFunction<double()>::Call, &get_time),
                         &stream);

  int dropped_frame_count = 0;
  bool is_new_frame = false;
  double delay = 0;

  // Time: 0
  auto ret = renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(ret, stream.GetFrame(0, FrameLocation::Near));
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, 0.01);
  step.Call();

  // Time: 0.03
  ret = renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(ret, stream.GetFrame(0.03, FrameLocation::Near));
  EXPECT_EQ(dropped_frame_count, 2);
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, 0.01);
}

TEST(VideoRendererTest, HandlesSeeks) {
  DecodedStream stream;
  stream.AddFrame(MakeFrame(0.00));
  stream.AddFrame(MakeFrame(0.01));
  stream.AddFrame(MakeFrame(0.02));
  stream.AddFrame(MakeFrame(0.03));
  stream.AddFrame(MakeFrame(0.04));

  MockFunction<double()> get_time;
  MockFunction<void()> step;

  {
    InSequence seq;
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0));
    EXPECT_CALL(step, Call()).Times(1);
    // Seek
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0.03));
  }

  VideoRenderer renderer(std::bind(&MockFunction<double()>::Call, &get_time),
                         &stream);

  int dropped_frame_count = 0;
  bool is_new_frame = false;
  double delay = 0;

  // Time: 0
  auto ret = renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(ret, stream.GetFrame(0, FrameLocation::Near));
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, 0.01);

  renderer.OnSeek();
  renderer.OnSeekDone();
  step.Call();

  // Time: 0.04
  ret = renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(ret, stream.GetFrame(0.03, FrameLocation::Near));
  EXPECT_EQ(dropped_frame_count, 0);  // Skipped over frames, but don't count.
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, 0.01);
}

TEST(VideoRendererTest, TracksNewFrames) {
  DecodedStream stream;
  stream.AddFrame(MakeFrame(0.00));
  stream.AddFrame(MakeFrame(0.02));
  stream.AddFrame(MakeFrame(0.04));

  MockFunction<double()> get_time;
  MockFunction<void()> step;

  {
    InSequence seq;
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0));
    EXPECT_CALL(step, Call()).Times(1);
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0.006));
    EXPECT_CALL(step, Call()).Times(1);
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0.006));
    EXPECT_CALL(step, Call()).Times(1);
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0.025));
    EXPECT_CALL(step, Call()).Times(1);
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0.031));
    EXPECT_CALL(step, Call()).Times(1);
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0.044));
  }

  VideoRenderer renderer(std::bind(&MockFunction<double()>::Call, &get_time),
                         &stream);

  int dropped_frame_count = 0;
  bool is_new_frame = false;
  double delay = 0;

#define FRAME_AT(i) (stream.GetFrame(i, FrameLocation::Near))

  // Time: 0
  auto ret = renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, 0.02);
  step.Call();

  for (int i = 0; i < 2; i++) {
    // Time: 0.006
    ret = renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
    EXPECT_EQ(ret, FRAME_AT(0));
    EXPECT_EQ(dropped_frame_count, 0);
    EXPECT_FALSE(is_new_frame);
    EXPECT_DOUBLE_EQ(delay, 0.014);
    step.Call();
  }

  // Time: 0.025
  ret = renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(ret, FRAME_AT(0.02));
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, 0.015);
  step.Call();

  // Time: 0.031
  ret = renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(ret, FRAME_AT(0.02));
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_FALSE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, 0.009);
  step.Call();

  // Time: 0.044
  ret = renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(ret, FRAME_AT(0.04));
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, kMinDelay);
#undef FRAME_AT
}

}  // namespace media
}  // namespace shaka
