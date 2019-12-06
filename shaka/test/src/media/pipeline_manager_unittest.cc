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

#include "src/media/pipeline_manager.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/util/clock.h"

namespace shaka {
namespace media {

namespace {

using namespace std::placeholders;  // NOLINT
using testing::_;
using testing::InSequence;
using testing::MockFunction;
using testing::NiceMock;
using testing::Return;

class MockClock : public util::Clock {
 public:
  MOCK_CONST_METHOD0(GetMonotonicTime, uint64_t());
  MOCK_CONST_METHOD1(SleepSeconds, void(double));
};

void IgnoreSeek() {}

}  // namespace

TEST(PipelineManagerTest, Initialization) {
  NiceMock<MockClock> clock;
  MockFunction<void(VideoPlaybackState)> client;
  auto callback = std::bind(&decltype(client)::Call, &client, _1);

  EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);

  PipelineManager pipeline(callback, &IgnoreSeek, &clock);
  EXPECT_EQ(pipeline.GetPlaybackState(), VideoPlaybackState::Initializing);
  pipeline.DoneInitializing();
  EXPECT_EQ(pipeline.GetPlaybackState(), VideoPlaybackState::Paused);
}

TEST(PipelineManagerTest, CalculatesCurrentTime) {
  NiceMock<MockClock> clock;
  NiceMock<MockFunction<void(VideoPlaybackState)>> client;
  auto callback = std::bind(&decltype(client)::Call, &client, _1);
  MockFunction<void(int)> task;

  {
    InSequence seq;
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(0));
    EXPECT_CALL(task, Call(1)).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(2 * 1000));
    EXPECT_CALL(task, Call(2)).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(3 * 1000));
    EXPECT_CALL(task, Call(3)).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(7 * 1000));
    EXPECT_CALL(task, Call(4)).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(9 * 1000));
    EXPECT_CALL(task, Call(5)).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(12 * 1000));
    EXPECT_CALL(task, Call(6)).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(13 * 1000));
  }

  PipelineManager pipeline(callback, &IgnoreSeek, &clock);
  ASSERT_EQ(pipeline.GetPlaybackRate(), 1);
  pipeline.DoneInitializing();
  pipeline.Play();
  pipeline.CanPlay();

  EXPECT_EQ(pipeline.GetCurrentTime(), 0);
  task.Call(1);
  EXPECT_EQ(pipeline.GetCurrentTime(), 2);
  task.Call(2);
  EXPECT_EQ(pipeline.GetCurrentTime(), 3);
  pipeline.Pause();
  task.Call(3);
  EXPECT_EQ(pipeline.GetCurrentTime(), 3);
  pipeline.Play();
  pipeline.CanPlay();
  task.Call(4);
  EXPECT_EQ(pipeline.GetCurrentTime(), 5);
  task.Call(5);
  pipeline.SetPlaybackRate(2);
  task.Call(6);
  EXPECT_EQ(pipeline.GetCurrentTime(), 10);
}

TEST(PipelineManagerTest, SeeksIfPastEndWhenSettingDuration) {
  NiceMock<MockClock> clock;
  MockFunction<void(VideoPlaybackState)> client;
  auto callback = std::bind(&decltype(client)::Call, &client, _1);
  MockFunction<void()> seek;
  auto on_seek = std::bind(&decltype(seek)::Call, &seek);

  {
    InSequence seq;
    EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);
    EXPECT_CALL(seek, Call()).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Seeking)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);
    EXPECT_CALL(seek, Call()).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Seeking)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Ended)).Times(1);
  }

  PipelineManager pipeline(callback, on_seek, &clock);
  pipeline.DoneInitializing();
  pipeline.SetCurrentTime(15);
  pipeline.CanPlay();  // Complete initial seek.
  pipeline.SetDuration(10);
  EXPECT_EQ(pipeline.GetCurrentTime(), 10);
  EXPECT_EQ(pipeline.GetDuration(), 10);
  pipeline.OnEnded();
  EXPECT_EQ(pipeline.GetPlaybackState(), VideoPlaybackState::Ended);
}

TEST(PipelineManagerTest, DoesntChangeStatusAfterErrors) {
  NiceMock<MockClock> clock;
  MockFunction<void(VideoPlaybackState)> client;
  auto callback = std::bind(&decltype(client)::Call, &client, _1);

  {
    InSequence seq;
    EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Errored)).Times(1);
  }

  PipelineManager pipeline(callback, &IgnoreSeek, &clock);
  pipeline.DoneInitializing();
  pipeline.OnError();
  pipeline.SetCurrentTime(15);
  pipeline.CanPlay();
  pipeline.OnEnded();
  pipeline.Play();
  EXPECT_EQ(pipeline.GetPlaybackState(), VideoPlaybackState::Errored);
  pipeline.Buffering();
  pipeline.Pause();
  EXPECT_EQ(pipeline.GetPlaybackState(), VideoPlaybackState::Errored);
  pipeline.OnError();
}


TEST(PipelineManagerTest, PlayPauseStall) {
  NiceMock<MockClock> clock;
  MockFunction<void(VideoPlaybackState)> client;
  auto callback = std::bind(&decltype(client)::Call, &client, _1);

  {
    InSequence seq;
    EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Buffering)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Playing)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Buffering)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);
  }

  PipelineManager pipeline(callback, &IgnoreSeek, &clock);
  pipeline.DoneInitializing();
  EXPECT_EQ(pipeline.GetPlaybackState(), VideoPlaybackState::Paused);
  pipeline.Play();
  pipeline.CanPlay();
  EXPECT_EQ(pipeline.GetPlaybackState(), VideoPlaybackState::Playing);
  pipeline.Buffering();
  pipeline.Pause();
  EXPECT_EQ(pipeline.GetPlaybackState(), VideoPlaybackState::Paused);
}

TEST(PipelineManagerTest, PlayingSeek) {
  NiceMock<MockClock> clock;
  MockFunction<void(VideoPlaybackState)> client;
  auto callback = std::bind(&decltype(client)::Call, &client, _1);
  MockFunction<void()> seek;
  auto on_seek = std::bind(&decltype(seek)::Call, &seek);

  {
    InSequence seq;
    EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Buffering)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Playing)).Times(1);
    EXPECT_CALL(seek, Call()).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Seeking)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Playing)).Times(1);
  }

  PipelineManager pipeline(callback, on_seek, &clock);
  pipeline.DoneInitializing();
  pipeline.Play();
  pipeline.CanPlay();
  pipeline.SetCurrentTime(10);
  pipeline.CanPlay();
}

TEST(PipelineManagerTest, PausedSeek) {
  NiceMock<MockClock> clock;
  MockFunction<void(VideoPlaybackState)> client;
  auto callback = std::bind(&decltype(client)::Call, &client, _1);
  MockFunction<void()> seek;
  auto on_seek = std::bind(&decltype(seek)::Call, &seek);

  {
    InSequence seq;
    EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);
    EXPECT_CALL(seek, Call()).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Seeking)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);
  }

  PipelineManager pipeline(callback, on_seek, &clock);
  pipeline.DoneInitializing();
  pipeline.SetCurrentTime(10);
  pipeline.CanPlay();
}

TEST(PipelineManagerTest, PlayingSeekPause) {
  NiceMock<MockClock> clock;
  MockFunction<void(VideoPlaybackState)> client;
  auto callback = std::bind(&decltype(client)::Call, &client, _1);
  MockFunction<void()> seek;
  auto on_seek = std::bind(&decltype(seek)::Call, &seek);

  {
    InSequence seq;
    EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);
    EXPECT_CALL(seek, Call()).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Seeking)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Playing)).Times(1);
  }

  PipelineManager pipeline(callback, on_seek, &clock);
  pipeline.DoneInitializing();
  pipeline.SetCurrentTime(10);
  pipeline.Play();
  pipeline.CanPlay();
}

TEST(PipelineManagerTest, Buffering) {
  NiceMock<MockClock> clock;
  MockFunction<void(VideoPlaybackState)> client;
  auto callback = std::bind(&decltype(client)::Call, &client, _1);
  MockFunction<void()> seek;
  auto on_seek = std::bind(&decltype(seek)::Call, &seek);

  {
    InSequence seq;
    EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Buffering)).Times(1);
    EXPECT_CALL(seek, Call()).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Seeking)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Playing)).Times(1);
  }

  PipelineManager pipeline(callback, on_seek, &clock);
  pipeline.DoneInitializing();
  pipeline.Play();
  pipeline.SetCurrentTime(10);
  pipeline.CanPlay();
}

TEST(PipelineManagerTest, SeekFiresMultipleTimes) {
  NiceMock<MockClock> clock;
  MockFunction<void(VideoPlaybackState)> client;
  auto callback = std::bind(&decltype(client)::Call, &client, _1);
  MockFunction<void()> seek;
  auto on_seek = std::bind(&decltype(seek)::Call, &seek);

  {
    InSequence seq;
    EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);
    EXPECT_CALL(seek, Call()).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Seeking)).Times(1);
    EXPECT_CALL(seek, Call()).Times(1);
  }

  PipelineManager pipeline(callback, on_seek, &clock);
  pipeline.DoneInitializing();
  pipeline.SetCurrentTime(10);
  pipeline.SetCurrentTime(20);
}


TEST(PipelineManagerTest, IgnoresSeeksBeforeStartup) {
  NiceMock<MockClock> clock;
  MockFunction<void(VideoPlaybackState)> client;
  auto callback = std::bind(&decltype(client)::Call, &client, _1);
  MockFunction<void()> seek;
  auto on_seek = std::bind(&decltype(seek)::Call, &seek);

  EXPECT_CALL(client, Call(_)).Times(0);
  EXPECT_CALL(seek, Call()).Times(0);
  EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);

  PipelineManager pipeline(callback, on_seek, &clock);
  pipeline.SetCurrentTime(50);
  pipeline.DoneInitializing();
  EXPECT_EQ(pipeline.GetCurrentTime(), 0);
}

TEST(PipelineManagerTest, SeekAfterEnd) {
  NiceMock<MockClock> clock;
  MockFunction<void(VideoPlaybackState)> client;
  auto callback = std::bind(&decltype(client)::Call, &client, _1);
  MockFunction<void()> seek;
  auto on_seek = std::bind(&decltype(seek)::Call, &seek);

  {
    InSequence seq;
    EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);
    EXPECT_CALL(seek, Call()).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Seeking)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Ended)).Times(1);
    EXPECT_CALL(seek, Call()).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Seeking)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);
  }

  PipelineManager pipeline(callback, on_seek, &clock);
  pipeline.SetDuration(10);
  pipeline.DoneInitializing();
  pipeline.SetCurrentTime(12);
  EXPECT_EQ(pipeline.GetCurrentTime(), 10);
  pipeline.OnEnded();
  EXPECT_EQ(pipeline.GetPlaybackState(), VideoPlaybackState::Ended);
  pipeline.SetCurrentTime(2);
  pipeline.CanPlay();
  EXPECT_EQ(pipeline.GetCurrentTime(), 2);
  EXPECT_EQ(pipeline.GetPlaybackState(), VideoPlaybackState::Paused);
}

TEST(PipelineManagerTest, PlayAfterEnd) {
  NiceMock<MockClock> clock;
  MockFunction<void(VideoPlaybackState)> client;
  auto callback = std::bind(&decltype(client)::Call, &client, _1);
  MockFunction<void()> seek;
  auto on_seek = std::bind(&decltype(seek)::Call, &seek);

  {
    InSequence seq;
    EXPECT_CALL(client, Call(VideoPlaybackState::Paused)).Times(1);
    EXPECT_CALL(seek, Call()).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Seeking)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Ended)).Times(1);
    EXPECT_CALL(seek, Call()).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Seeking)).Times(1);
    EXPECT_CALL(client, Call(VideoPlaybackState::Playing)).Times(1);
  }

  PipelineManager pipeline(callback, on_seek, &clock);
  pipeline.SetDuration(10);
  pipeline.DoneInitializing();
  pipeline.SetCurrentTime(12);
  EXPECT_EQ(pipeline.GetCurrentTime(), 10);
  pipeline.OnEnded();
  EXPECT_EQ(pipeline.GetPlaybackState(), VideoPlaybackState::Ended);
  pipeline.Play();
  pipeline.CanPlay();
  EXPECT_EQ(pipeline.GetCurrentTime(), 0);
  EXPECT_EQ(pipeline.GetPlaybackState(), VideoPlaybackState::Playing);
}

}  // namespace media
}  // namespace shaka
