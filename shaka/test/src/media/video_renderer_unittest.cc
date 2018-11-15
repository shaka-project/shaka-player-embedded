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

#include "src/media/base_frame.h"
#include "src/media/stream.h"

namespace shaka {
namespace media {

using testing::_;
using testing::InSequence;
using testing::MockFunction;
using testing::Return;

namespace {

class MockFrameDrawer : public FrameDrawer {
 public:
  MOCK_METHOD1(DrawFrame, Frame(const BaseFrame* frame));
};

std::unique_ptr<BaseFrame> MakeFrame(double start) {
  BaseFrame* ret = new BaseFrame(start, start, 0.01, true);
  return std::unique_ptr<BaseFrame>(ret);
}

constexpr const double kMinDelay = 1.0 / 120;

}  // namespace

class VideoRendererTest : public testing::Test {
 protected:
  void SetDrawer(VideoRenderer* renderer, FrameDrawer* drawer) {
    renderer->SetDrawerForTesting(std::unique_ptr<FrameDrawer>(drawer));
  }
};

TEST_F(VideoRendererTest, WorksWithNoNextFrame) {
  Stream stream;
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.00));

  MockFunction<double()> get_time;
  auto* drawer = new MockFrameDrawer;

  {
#define FRAME_AT(i) (stream.GetDecodedFrames()->GetFrameNear(i).get())
    InSequence seq;
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0));
    EXPECT_CALL(*drawer, DrawFrame(FRAME_AT(0))).Times(1);
#undef GET_FRAME
  }

  VideoRenderer renderer(std::bind(&MockFunction<double()>::Call, &get_time),
                         &stream);
  SetDrawer(&renderer, drawer);  // Takes ownership.

  int dropped_frame_count = 0;
  bool is_new_frame = false;
  double delay = 0;
  renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_TRUE(is_new_frame);
  EXPECT_EQ(delay, kMinDelay);
}

TEST_F(VideoRendererTest, WorksWithNoFrames) {
  Stream stream;

  MockFunction<double()> get_time;
  auto* drawer = new MockFrameDrawer;

  {
    InSequence seq;
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0));
    EXPECT_CALL(*drawer, DrawFrame(_)).Times(0);
  }

  VideoRenderer renderer(std::bind(&MockFunction<double()>::Call, &get_time),
                         &stream);
  SetDrawer(&renderer, drawer);  // Takes ownership.

  int dropped_frame_count = 0;
  bool is_new_frame = false;
  double delay = 0;
  renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
}

TEST_F(VideoRendererTest, DrawsFrameInPast) {
  Stream stream;
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.00));

  MockFunction<double()> get_time;
  auto* drawer = new MockFrameDrawer;

  {
#define FRAME_AT(i) (stream.GetDecodedFrames()->GetFrameNear(i).get())
    InSequence seq;
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(4));
    EXPECT_CALL(*drawer, DrawFrame(FRAME_AT(0))).Times(1);
#undef GET_FRAME
  }

  VideoRenderer renderer(std::bind(&MockFunction<double()>::Call, &get_time),
                         &stream);
  SetDrawer(&renderer, drawer);  // Takes ownership.

  int dropped_frame_count = 0;
  bool is_new_frame = false;
  double delay = 0;
  renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, kMinDelay);
}

TEST_F(VideoRendererTest, WillDropFrames) {
  Stream stream;
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.00));
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.01));
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.02));
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.03));
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.04));

  MockFunction<double()> get_time;
  auto* drawer = new MockFrameDrawer;

  {
#define FRAME_AT(i) (stream.GetDecodedFrames()->GetFrameNear(i).get())
    InSequence seq;
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0));
    EXPECT_CALL(*drawer, DrawFrame(FRAME_AT(0))).Times(1);
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0.03));
    EXPECT_CALL(*drawer, DrawFrame(FRAME_AT(0.03))).Times(1);
#undef GET_FRAME
  }

  VideoRenderer renderer(std::bind(&MockFunction<double()>::Call, &get_time),
                         &stream);
  SetDrawer(&renderer, drawer);  // Takes ownership.

  int dropped_frame_count = 0;
  bool is_new_frame = false;
  double delay = 0;

  // Time: 0
  renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, 0.01);

  // Time: 0.03
  renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(dropped_frame_count, 2);
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, 0.01);
}

TEST_F(VideoRendererTest, HandlesSeeks) {
  Stream stream;
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.00));
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.01));
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.02));
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.03));
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.04));

  MockFunction<double()> get_time;
  auto* drawer = new MockFrameDrawer;

  {
#define FRAME_AT(i) (stream.GetDecodedFrames()->GetFrameNear(i).get())
    InSequence seq;
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0));
    EXPECT_CALL(*drawer, DrawFrame(FRAME_AT(0))).Times(1);
    // Seek
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0.03));
    EXPECT_CALL(*drawer, DrawFrame(FRAME_AT(0.03))).Times(1);
#undef GET_FRAME
  }

  VideoRenderer renderer(std::bind(&MockFunction<double()>::Call, &get_time),
                         &stream);
  SetDrawer(&renderer, drawer);  // Takes ownership.

  int dropped_frame_count = 0;
  bool is_new_frame = false;
  double delay = 0;

  // Time: 0
  renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, 0.01);

  renderer.OnSeek();
  renderer.OnSeekDone();

  // Time: 0.04
  renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(dropped_frame_count, 0);  // Skipped over frames, but don't count.
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, 0.01);
}

TEST_F(VideoRendererTest, TracksNewFrames) {
  Stream stream;
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.00));
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.02));
  stream.GetDecodedFrames()->AppendFrame(MakeFrame(0.04));

  MockFunction<double()> get_time;
  auto* drawer = new MockFrameDrawer;

  {
#define FRAME_AT(i) (stream.GetDecodedFrames()->GetFrameNear(i).get())
    InSequence seq;
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0));
    EXPECT_CALL(*drawer, DrawFrame(FRAME_AT(0))).Times(1);
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0.006));
    EXPECT_CALL(*drawer, DrawFrame(FRAME_AT(0))).Times(1);
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0.006));
    EXPECT_CALL(*drawer, DrawFrame(FRAME_AT(0))).Times(1);
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0.025));
    EXPECT_CALL(*drawer, DrawFrame(FRAME_AT(0.02))).Times(1);
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0.031));
    EXPECT_CALL(*drawer, DrawFrame(FRAME_AT(0.02))).Times(1);
    EXPECT_CALL(get_time, Call()).WillRepeatedly(Return(0.044));
    EXPECT_CALL(*drawer, DrawFrame(FRAME_AT(0.04))).Times(1);
#undef GET_FRAME
  }

  VideoRenderer renderer(std::bind(&MockFunction<double()>::Call, &get_time),
                         &stream);
  SetDrawer(&renderer, drawer);  // Takes ownership.

  int dropped_frame_count = 0;
  bool is_new_frame = false;
  double delay = 0;

  // Time: 0
  renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, 0.02);

  for (int i = 0; i < 2; i++) {
    // Time: 0.006
    renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
    EXPECT_EQ(dropped_frame_count, 0);
    EXPECT_FALSE(is_new_frame);
    EXPECT_DOUBLE_EQ(delay, 0.014);
  }

  // Time: 0.025
  renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, 0.015);

  // Time: 0.031
  renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_FALSE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, 0.009);

  // Time: 0.044
  renderer.DrawFrame(&dropped_frame_count, &is_new_frame, &delay);
  EXPECT_EQ(dropped_frame_count, 0);
  EXPECT_TRUE(is_new_frame);
  EXPECT_DOUBLE_EQ(delay, kMinDelay);
}

}  // namespace media
}  // namespace shaka
