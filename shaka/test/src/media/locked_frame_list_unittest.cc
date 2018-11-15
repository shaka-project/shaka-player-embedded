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

#include "src/media/locked_frame_list.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace shaka {
namespace media {

namespace {

std::unique_ptr<BaseFrame> MakeFrame() {
  return std::unique_ptr<BaseFrame>(new BaseFrame(0, 0, 1, true));
}

}  // namespace

using testing::InSequence;
using testing::MockFunction;

TEST(LockedFrameListTest, CanGuardFrames) {
  std::unique_ptr<BaseFrame> frame = MakeFrame();
  LockedFrameList list;

  auto guard = list.GuardFrame(frame.get());
  EXPECT_EQ(guard.get(), frame.get());
}

TEST(LockedFrameListTest, WillWaitForDelete) {
  MockFunction<void(int)> task;

  {
    InSequence seq;
    EXPECT_CALL(task, Call(1)).Times(1);
    EXPECT_CALL(task, Call(10)).Times(1);
    EXPECT_CALL(task, Call(11)).Times(1);
    EXPECT_CALL(task, Call(2)).Times(1);
    EXPECT_CALL(task, Call(12)).Times(1);
  }

  std::unique_ptr<BaseFrame> frame1 = MakeFrame();
  std::unique_ptr<BaseFrame> frame2 = MakeFrame();
  LockedFrameList list;
  std::mutex mutex;  // Protects the frames, similar to FrameBuffer.
  ThreadEvent<void> delete_start("");
  ThreadEvent<void> use_frame("");

  auto get_guard = [&]() {
    std::unique_lock<std::mutex> lock(mutex);
    return list.GuardFrame(frame1.get());
  };

  std::thread user([&]() {
    LockedFrameList::Guard guard = get_guard();
    task.Call(1);
    delete_start.SignalAll();
    use_frame.GetValue();
    usleep(2500);
    task.Call(2);
  });

  std::thread deleter([&]() {
    delete_start.GetValue();

    std::unique_lock<std::mutex> lock(mutex);
    task.Call(10);

    // Should not have to wait for unrelated frames.
    list.WaitToDeleteFrames({frame2.get()});
    task.Call(11);

    // Should have to wait for locked frame.
    use_frame.SignalAll();
    list.WaitToDeleteFrames({frame1.get()});
    task.Call(12);
  });

  user.join();
  deleter.join();
}


}  // namespace media
}  // namespace shaka
