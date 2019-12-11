// Copyright 2018 Google LLC
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

#include "src/core/task_runner.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/debug/thread_event.h"
#include "src/memory/heap_tracer.h"

namespace shaka {

namespace {

using testing::_;
using testing::InSequence;
using testing::MockFunction;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

class MockClock : public util::Clock {
 public:
  MOCK_CONST_METHOD0(GetMonotonicTime, uint64_t());
  MOCK_CONST_METHOD1(SleepSeconds, void(double));
};

class TaskWatcher {
 public:
  MOCK_METHOD0(Call, void());
  MOCK_METHOD1(Trace, void(memory::HeapTracer*));
};

class MockTask : public memory::Traceable {
 public:
  MockTask(TaskWatcher* watcher) : watcher_(watcher) {}

  void operator()() {
    watcher_->Call();
  }

  void Trace(memory::HeapTracer* tracer) const override {
    watcher_->Trace(tracer);
  }

 private:
  TaskWatcher* watcher_;
};

}  // namespace

TEST(TaskRunnerTest, DelaysTimers) {
  StrictMock<TaskWatcher> watcher;
  NiceMock<MockClock> clock;
  MockFunction<void()> start;

  {
    InSequence seq;
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(10));
    EXPECT_CALL(start, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillOnce(Return(10));
    EXPECT_CALL(clock, GetMonotonicTime()).WillOnce(Return(12));
    EXPECT_CALL(clock, GetMonotonicTime()).WillOnce(Return(16));
    EXPECT_CALL(clock, GetMonotonicTime()).WillOnce(Return(19));
    EXPECT_CALL(clock, GetMonotonicTime()).WillOnce(Return(19));
    EXPECT_CALL(clock, GetMonotonicTime()).WillOnce(Return(20));
    EXPECT_CALL(watcher, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(21));
  }

  TaskRunner runner([](TaskRunner::RunLoop loop) { loop(); }, &clock, true);
  runner.AddTimer(10, MockTask(&watcher));
  start.Call();
  runner.WaitUntilFinished();
}

TEST(TaskRunnerTest, FiresTimersBasedOnRegisterOrder) {
  StrictMock<TaskWatcher> watcher1;
  StrictMock<TaskWatcher> watcher2;
  NiceMock<MockClock> clock;
  MockFunction<void()> start;

  {
    InSequence seq;
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(0));
    EXPECT_CALL(start, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(10));
    EXPECT_CALL(watcher1, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(10));
    EXPECT_CALL(watcher2, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(10));
  }

  TaskRunner runner([](TaskRunner::RunLoop loop) { loop(); }, &clock, true);
  runner.AddTimer(5, MockTask(&watcher1));
  runner.AddTimer(5, MockTask(&watcher2));
  start.Call();
  runner.WaitUntilFinished();
}

TEST(TaskRunnerTest, FiresSmallerTimersFirst) {
  StrictMock<TaskWatcher> watcher1;
  StrictMock<TaskWatcher> watcher2;
  NiceMock<MockClock> clock;
  MockFunction<void()> start;

  {
    InSequence seq;
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(0));
    EXPECT_CALL(start, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(10));
    EXPECT_CALL(watcher2, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(10));
    EXPECT_CALL(watcher1, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(10));
  }

  TaskRunner runner([](TaskRunner::RunLoop loop) { loop(); }, &clock, true);
  runner.AddTimer(7, MockTask(&watcher1));
  runner.AddTimer(2, MockTask(&watcher2));
  start.Call();
  runner.WaitUntilFinished();
}

TEST(TaskRunnerTest, FiresZeroDelayTimers) {
  StrictMock<TaskWatcher> watcher;
  NiceMock<MockClock> clock;

  ON_CALL(clock, GetMonotonicTime()).WillByDefault(Return(0));
  EXPECT_CALL(watcher, Call()).Times(1);

  TaskRunner runner([](TaskRunner::RunLoop loop) { loop(); }, &clock, true);
  runner.AddTimer(0, MockTask(&watcher));
  runner.WaitUntilFinished();
}

TEST(TaskRunnerTest, FiresRepeatedTimers) {
  StrictMock<TaskWatcher> watcher;
  NiceMock<MockClock> clock;
  MockFunction<void()> start;

  {
    InSequence seq;
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(0));
    EXPECT_CALL(start, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillOnce(Return(2));
    EXPECT_CALL(clock, GetMonotonicTime()).WillOnce(Return(6));
    EXPECT_CALL(clock, GetMonotonicTime()).WillOnce(Return(9));
    EXPECT_CALL(clock, GetMonotonicTime()).WillOnce(Return(10));
    EXPECT_CALL(watcher, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillOnce(Return(10));
    EXPECT_CALL(clock, GetMonotonicTime()).WillOnce(Return(12));
    EXPECT_CALL(clock, GetMonotonicTime()).WillOnce(Return(19));
    EXPECT_CALL(clock, GetMonotonicTime()).WillOnce(Return(21));
    EXPECT_CALL(watcher, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(25));
  }

  TaskRunner runner([](TaskRunner::RunLoop loop) { loop(); }, &clock, true);
  runner.AddRepeatedTimer(10, MockTask(&watcher));
  start.Call();
  util::Clock::Instance.SleepSeconds(0.01);
  runner.Stop();
}

TEST(TaskRunnerTest, CancelsPendingTimers) {
  StrictMock<TaskWatcher> watcher;
  NiceMock<MockClock> clock;
  MockFunction<void()> start;

  EXPECT_CALL(watcher, Call()).Times(0);
  {
    InSequence seq;
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(0));
    EXPECT_CALL(start, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(10));
  }

  TaskRunner runner([](TaskRunner::RunLoop loop) { loop(); }, &clock, true);
  int id = runner.AddTimer(5, MockTask(&watcher));
  util::Clock::Instance.SleepSeconds(0.001);
  runner.CancelTimer(id);
  start.Call();
  runner.WaitUntilFinished();
}

TEST(TaskRunnerTest, CancelsRepeatedTimers) {
  StrictMock<TaskWatcher> watcher;
  NiceMock<MockClock> clock;
  MockFunction<void(int)> start;

  {
    InSequence seq;
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(0));
    EXPECT_CALL(start, Call(1)).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(12));
    EXPECT_CALL(watcher, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(12));
    EXPECT_CALL(start, Call(2)).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(22));
  }

  TaskRunner runner([](TaskRunner::RunLoop loop) { loop(); }, &clock, true);
  int id = runner.AddRepeatedTimer(10, MockTask(&watcher));
  start.Call(1);
  util::Clock::Instance.SleepSeconds(0.001);
  runner.CancelTimer(id);
  start.Call(2);
  runner.WaitUntilFinished();
}

TEST(TaskRunnerTest, IgnoresUnknownWhenCanceling) {
  StrictMock<TaskWatcher> watcher;
  NiceMock<MockClock> clock;
  MockFunction<void()> start;

  {
    InSequence seq;
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(0));
    EXPECT_CALL(start, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(10));
    EXPECT_CALL(watcher, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(10));
  }

  TaskRunner runner([](TaskRunner::RunLoop loop) { loop(); }, &clock, true);
  int id = runner.AddTimer(5, MockTask(&watcher));
  runner.CancelTimer(id + 1000);
  runner.CancelTimer(id + 55);
  runner.CancelTimer(id + 55);
  runner.CancelTimer(id - 22);
  start.Call();
  runner.WaitUntilFinished();
}

TEST(TaskRunnerTest, TracesPendingEvents) {
  StrictMock<TaskWatcher> watcher;
  NiceMock<MockClock> clock;
  memory::HeapTracer tracer;
  memory::ObjectTracker::UnsetForTesting unset;
  memory::ObjectTracker object_tracker;

  ON_CALL(clock, GetMonotonicTime()).WillByDefault(Return(0));
  {
    InSequence seq;
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(0));
    EXPECT_CALL(watcher, Trace(&tracer)).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(10));
    EXPECT_CALL(watcher, Call()).Times(1);
    EXPECT_CALL(clock, GetMonotonicTime()).WillRepeatedly(Return(10));
  }

  TaskRunner runner([](TaskRunner::RunLoop loop) { loop(); }, &clock, true);
  runner.AddTimer(5, MockTask(&watcher));
  util::Clock::Instance.SleepSeconds(0.001);
  tracer.TraceCommon(object_tracker.GetAliveObjects());
  util::Clock::Instance.SleepSeconds(0.001);
  tracer.TraceCommon(object_tracker.GetAliveObjects());
  util::Clock::Instance.SleepSeconds(0.001);
  runner.Stop();

  object_tracker.Dispose();
}


TEST(TaskRunnerTest, OrdersInternalTasks) {
  StrictMock<TaskWatcher> watcher1;
  StrictMock<TaskWatcher> watcher2;
  NiceMock<MockClock> clock;

  {
    InSequence seq;
    EXPECT_CALL(watcher2, Call()).Times(1);
    EXPECT_CALL(watcher1, Call()).Times(1);
  }

  ThreadEvent<void> delay("");
  TaskRunner runner(
      [&](TaskRunner::RunLoop loop) {
        delay.GetValue();
        loop();
      },
      &clock, true);
  runner.AddInternalTask(TaskPriority::Internal, "", MockTask(&watcher1));
  runner.AddInternalTask(TaskPriority::Immediate, "", MockTask(&watcher2));
  delay.SignalAll();
  runner.WaitUntilFinished();
}

TEST(TaskRunnerTest, FiresInternalTasksBeforeTimers) {
  StrictMock<TaskWatcher> watcher1;
  StrictMock<TaskWatcher> watcher2;
  NiceMock<MockClock> clock;

  {
    InSequence seq;
    EXPECT_CALL(watcher2, Call()).Times(1);
    EXPECT_CALL(watcher1, Call()).Times(1);
  }

  ThreadEvent<void> delay("");
  TaskRunner runner(
      [&](TaskRunner::RunLoop loop) {
        delay.GetValue();
        loop();
      },
      &clock, true);
  runner.AddTimer(0, MockTask(&watcher1));
  runner.AddInternalTask(TaskPriority::Internal, "", MockTask(&watcher2));
  delay.SignalAll();
  runner.WaitUntilFinished();
}

TEST(TaskRunnerTest, PassesReturnValues) {
  std::function<double()> cb = []() { return 1234.5; };
  NiceMock<MockClock> clock;

  TaskRunner runner([](TaskRunner::RunLoop loop) { loop(); }, &clock, true);
  auto data =
      runner.AddInternalTask(TaskPriority::Internal, "", PlainCallbackTask(cb));
  EXPECT_EQ(1234.5, data->GetValue());
}

}  // namespace shaka
