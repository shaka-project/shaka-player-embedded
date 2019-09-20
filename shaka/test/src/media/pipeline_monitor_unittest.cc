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

#include "src/media/pipeline_monitor.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <math.h>

#include "src/media/pipeline_manager.h"
#include "src/util/clock.h"

namespace shaka {
namespace media {

namespace {

using testing::_;
using testing::AtLeast;
using testing::InSequence;
using testing::MockFunction;
using testing::NiceMock;
using testing::Return;
using testing::Sequence;
using testing::StrictMock;

class MockClock : public util::Clock {
 public:
  MOCK_CONST_METHOD0(GetMonotonicTime, uint64_t());
  MOCK_CONST_METHOD1(SleepSeconds, void(double));
};

class MockPipelineManager : public PipelineManager {
 public:
  MockPipelineManager(const util::Clock* clock)
      : PipelineManager({}, {}, clock) {}

  MOCK_METHOD0(DoneInitializing, void());
  MOCK_CONST_METHOD0(GetPipelineStatus, PipelineStatus());
  MOCK_CONST_METHOD0(GetDuration, double());
  MOCK_METHOD1(SetDuration, void(double));
  MOCK_CONST_METHOD0(GetCurrentTime, double());
  MOCK_METHOD1(SetCurrentTime, void(double));
  MOCK_CONST_METHOD0(GetPlaybackRate, double());
  MOCK_METHOD1(SetPlaybackRate, void(double));
  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD0(Stalled, void());
  MOCK_METHOD0(CanPlay, void());
  MOCK_METHOD0(OnEnded, void());
};

#define CALLBACK(var) std::bind(&decltype(var)::Call, &var)
#define CALLBACK1(var) \
  std::bind(&decltype(var)::Call, &var, std::placeholders::_1)

}  // namespace

TEST(PipelineMonitorTest, ChangesReadyState) {
  NiceMock<MockClock> clock;
  NiceMock<MockPipelineManager> pipeline(&clock);
  MockFunction<BufferedRanges()> get_buffered;
  MockFunction<void(MediaReadyState)> ready_state_changed;

  EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(0));
  EXPECT_CALL(pipeline, GetDuration()).WillRepeatedly(Return(NAN));
  EXPECT_CALL(pipeline, GetPipelineStatus())
      .WillRepeatedly(Return(PipelineStatus::Paused));
  {
#define SET_BUFFERED_RANGE(start, end) \
  EXPECT_CALL(get_buffered, Call())    \
      .WillRepeatedly(Return(BufferedRanges{{start, end}}));
#define SET_NOTHING_BUFFERED() \
  EXPECT_CALL(get_buffered, Call()).WillRepeatedly(Return(BufferedRanges()));
    InSequence seq;
    SET_BUFFERED_RANGE(0, 10);
    EXPECT_CALL(ready_state_changed, Call(HAVE_FUTURE_DATA)).Times(1);
    SET_BUFFERED_RANGE(0, 0);
    EXPECT_CALL(ready_state_changed, Call(HAVE_CURRENT_DATA)).Times(1);
    SET_BUFFERED_RANGE(0, 10);
    EXPECT_CALL(ready_state_changed, Call(HAVE_FUTURE_DATA)).Times(1);
    SET_NOTHING_BUFFERED();
    EXPECT_CALL(ready_state_changed, Call(HAVE_METADATA)).Times(1);
    SET_BUFFERED_RANGE(0, 0);
    EXPECT_CALL(ready_state_changed, Call(HAVE_CURRENT_DATA)).Times(1);

    SET_BUFFERED_RANGE(0, 0);
#undef SET_BUFFERED_RANGE
#undef SET_NOTHING_BUFFERED
  }

  PipelineMonitor monitor(CALLBACK(get_buffered), CALLBACK(get_buffered),
                          CALLBACK1(ready_state_changed), &clock, &pipeline);
  util::Clock::Instance.SleepSeconds(0.01);
  monitor.Stop();
}

TEST(PipelineMonitorTest, ChangesPiplineStatuses) {
  NiceMock<MockClock> clock;
  StrictMock<MockPipelineManager> pipeline(&clock);
  MockFunction<BufferedRanges()> get_buffered;
  NiceMock<MockFunction<void(MediaReadyState)>> ready_state_changed;

  EXPECT_CALL(pipeline, GetPipelineStatus())
      .WillRepeatedly(Return(PipelineStatus::Paused));
  EXPECT_CALL(pipeline, GetDuration()).WillRepeatedly(Return(10));
  EXPECT_CALL(get_buffered, Call())
      .WillRepeatedly(Return(BufferedRanges{{0, 4}, {6, 10}}));
  {
    // What we want is to say: these things must appear in this order, then
    // the next calls can appear in any order.  Unfortunately I don't know of a
    // way to do this easily.  So this creates two sequences to allow the two
    // calls at the end to be in any order WRT each other.
    Sequence seq1, seq2;
    EXPECT_CALL(pipeline, GetCurrentTime())
        .InSequence(seq1, seq2)
        .WillRepeatedly(Return(0));
    EXPECT_CALL(pipeline, CanPlay()).Times(1).InSequence(seq1, seq2);
    EXPECT_CALL(pipeline, GetCurrentTime())
        .InSequence(seq1, seq2)
        .WillRepeatedly(Return(3));
    EXPECT_CALL(pipeline, CanPlay()).Times(1).InSequence(seq1, seq2);
    EXPECT_CALL(pipeline, GetCurrentTime())
        .InSequence(seq1, seq2)
        .WillRepeatedly(Return(5));
    EXPECT_CALL(pipeline, Stalled()).Times(1).InSequence(seq1, seq2);
    EXPECT_CALL(pipeline, GetCurrentTime())
        .InSequence(seq1, seq2)
        .WillRepeatedly(Return(8));
    EXPECT_CALL(pipeline, CanPlay()).Times(1).InSequence(seq1, seq2);
    EXPECT_CALL(pipeline, GetCurrentTime())
        .InSequence(seq1, seq2)
        .WillRepeatedly(Return(10));
    EXPECT_CALL(pipeline, OnEnded()).Times(1).InSequence(seq1, seq2);

    // Defaults when sequence is done.
    EXPECT_CALL(pipeline, GetCurrentTime())
        .InSequence(seq1)
        .WillRepeatedly(Return(10));
    EXPECT_CALL(pipeline, OnEnded()).Times(AtLeast(1)).InSequence(seq2);
  }

  PipelineMonitor monitor(CALLBACK(get_buffered), CALLBACK(get_buffered),
                          CALLBACK1(ready_state_changed), &clock, &pipeline);
  util::Clock::Instance.SleepSeconds(0.01);
  monitor.Stop();
}

}  // namespace media
}  // namespace shaka
