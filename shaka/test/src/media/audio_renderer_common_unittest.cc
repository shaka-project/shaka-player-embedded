// Copyright 2020 Google LLC
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

#include "src/media/audio_renderer_common.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "shaka/media/frames.h"
#include "shaka/media/streams.h"
#include "src/debug/thread_event.h"
#include "src/util/clock.h"

namespace shaka {
namespace media {

namespace {

using testing::_;
using testing::Args;
using testing::AtLeast;
using testing::InSequence;
using testing::InvokeWithoutArgs;
using testing::MockFunction;
using testing::NiceMock;
using testing::Return;
using testing::ReturnPointee;
using testing::SaveArg;
using testing::StrictMock;

const size_t kSampleRate = 2;
const uint8_t kData1[] = {1, 2, 3, 4};
const uint8_t kData2[] = {5, 6, 7, 8};
const uint8_t kData3[] = {9, 10};

#define WAIT_WITH_TIMEOUT(cond)                                       \
  EXPECT_EQ((cond).future().wait_for(std::chrono::milliseconds(100)), \
            std::future_status::ready)

#define SignalAndReturn(cond, val) \
  InvokeWithoutArgs([&]() {        \
    cond.SignalAll();              \
    return (val);                  \
  })

std::shared_ptr<StreamInfo> MakeStreamInfo() {
  return std::shared_ptr<StreamInfo>{
      new StreamInfo("", "", false, {0, 0}, {0, 0}, {}, 0, 0, 1, kSampleRate)};
}

template <size_t N>
std::shared_ptr<DecodedFrame> MakeFrame(std::shared_ptr<StreamInfo> stream_info,
                                        double start,
                                        const uint8_t (&array)[N]) {
  auto* ret = new DecodedFrame(stream_info, start, start, 0.01,
                               SampleFormat::PackedU8, 0, {array}, {N});
  return std::shared_ptr<DecodedFrame>(ret);
}

class MockMediaPlayer : public MediaPlayer {
 public:
  MOCK_CONST_METHOD1(DecodingInfo,
                     MediaCapabilitiesInfo(const MediaDecodingConfiguration&));
  MOCK_CONST_METHOD0(VideoPlaybackQuality, struct VideoPlaybackQuality());
  MOCK_CONST_METHOD1(AddClient, void(Client*));
  MOCK_CONST_METHOD1(RemoveClient, void(Client*));
  MOCK_CONST_METHOD0(GetBuffered, std::vector<BufferedRange>());
  MOCK_CONST_METHOD0(ReadyState, VideoReadyState());
  MOCK_CONST_METHOD0(PlaybackState, VideoPlaybackState());
  MOCK_METHOD0(AudioTracks, std::vector<std::shared_ptr<MediaTrack>>());
  MOCK_CONST_METHOD0(AudioTracks,
                     std::vector<std::shared_ptr<const MediaTrack>>());
  MOCK_METHOD0(VideoTracks, std::vector<std::shared_ptr<MediaTrack>>());
  MOCK_CONST_METHOD0(VideoTracks,
                     std::vector<std::shared_ptr<const MediaTrack>>());
  MOCK_METHOD0(TextTracks, std::vector<std::shared_ptr<TextTrack>>());
  MOCK_CONST_METHOD0(TextTracks,
                     std::vector<std::shared_ptr<const TextTrack>>());
  MOCK_METHOD3(AddTextTrack,
               std::shared_ptr<TextTrack>(TextTrackKind, const std::string&,
                                          const std::string&));
  MOCK_METHOD1(SetVideoFillMode, bool(VideoFillMode));
  MOCK_CONST_METHOD0(Width, uint32_t());
  MOCK_CONST_METHOD0(Height, uint32_t());
  MOCK_CONST_METHOD0(Volume, double());
  MOCK_METHOD1(SetVolume, void(double));
  MOCK_CONST_METHOD0(Muted, bool());
  MOCK_METHOD1(SetMuted, void(bool));
  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_CONST_METHOD0(CurrentTime, double());
  MOCK_METHOD1(SetCurrentTime, void(double));
  MOCK_CONST_METHOD0(Duration, double());
  MOCK_METHOD1(SetDuration, void(double));
  MOCK_CONST_METHOD0(PlaybackRate, double());
  MOCK_METHOD1(SetPlaybackRate, void(double));
  MOCK_METHOD1(AttachSource, bool(const std::string&));
  MOCK_METHOD0(AttachMse, bool());
  MOCK_METHOD3(AddMseBuffer,
               bool(const std::string&, bool, const ElementaryStream*));
  MOCK_METHOD1(LoadedMetaData, void(double));
  MOCK_METHOD0(MseEndOfStream, void());
  MOCK_METHOD2(SetEmeImplementation,
               bool(const std::string&, eme::Implementation*));
  MOCK_METHOD0(Detach, void());
};

class MockClock : public util::Clock {
 public:
  MOCK_CONST_METHOD0(GetMonotonicTime, uint64_t());
  MOCK_CONST_METHOD0(GetEpochTime, uint64_t());
  MOCK_CONST_METHOD1(SleepSeconds, void(double));
};

class TestAudioRenderer : public AudioRendererCommon {
 public:
  ~TestAudioRenderer() override {
    Stop();
  }

  // Stop is normally protected; redefine it here to expose to tests.
  void Stop() {
    AudioRendererCommon::Stop();
  }

  MOCK_METHOD2(InitDevice, bool(std::shared_ptr<DecodedFrame>, double));
  MOCK_METHOD2(AppendBuffer, bool(const uint8_t*, size_t));
  MOCK_METHOD0(ClearBuffer, void());
  MOCK_CONST_METHOD0(GetBytesBuffered, size_t());
  MOCK_METHOD1(SetDeviceState, void(bool));
  MOCK_METHOD1(UpdateVolume, void(double));
};

MATCHER_P(MatchesData, data, "") {
  using std::get;
  *result_listener << "where the data is: "
                   << testing::PrintToString(std::vector<uint8_t>(
                          get<0>(arg), get<0>(arg) + get<1>(arg)));
  return memcmp(get<0>(arg), data, get<1>(arg)) == 0;
}

}  // namespace

class AudioRendererCommonTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(player, PlaybackState())
        .WillByDefault(Return(VideoPlaybackState::Playing));
    ON_CALL(player, CurrentTime()).WillByDefault(Return(0));
    ON_CALL(player, PlaybackRate()).WillByDefault(Return(1));

    ON_CALL(renderer, InitDevice(_, _)).WillByDefault(Return(true));
    ON_CALL(renderer, AppendBuffer(_, _)).WillByDefault(Return(true));
    EXPECT_CALL(player, AddClient(_)).WillOnce(SaveArg<0>(&player_client));

    renderer.SetPlayer(&player);
    renderer.SetClock(&clock);
  }

  DecodedStream stream;
  NiceMock<MockClock> clock;
  NiceMock<MockMediaPlayer> player;
  MediaPlayer::Client* player_client;
  NiceMock<TestAudioRenderer> renderer;
};

TEST_F(AudioRendererCommonTest, WaitsForFrames) {
  auto info = MakeStreamInfo();
  auto frame = MakeFrame(info, 0, kData1);

  MockFunction<void()> step;
  ThreadEvent<void> did_append("");
  {
    InSequence seq;
    EXPECT_CALL(step, Call()).Times(1);
    EXPECT_CALL(renderer, InitDevice(frame, 1)).Times(1);
    EXPECT_CALL(renderer, AppendBuffer(kData1, sizeof(kData1)))
        .WillOnce(SignalAndReturn(did_append, true));
  }

  renderer.Attach(&stream);

  util::Clock::Instance.SleepSeconds(0.01);
  step.Call();
  stream.AddFrame(frame);
  WAIT_WITH_TIMEOUT(did_append);
}

TEST_F(AudioRendererCommonTest, ChangesVolumeAndMuted) {
  auto info = MakeStreamInfo();
  stream.AddFrame(MakeFrame(info, 0, kData1));

  ThreadEvent<void> did_init("");
  {
    InSequence seq;
    EXPECT_CALL(renderer, UpdateVolume(0.8)).Times(1);
    // Volume is set a second time when we change the volume while muted.
    EXPECT_CALL(renderer, UpdateVolume(0)).Times(2);
    EXPECT_CALL(renderer, UpdateVolume(0.2)).Times(1);
    EXPECT_CALL(renderer, InitDevice(_, 0.2))
        .WillOnce(SignalAndReturn(did_init, true));
  }

  EXPECT_EQ(1, renderer.Volume());
  EXPECT_EQ(false, renderer.Muted());
  renderer.SetVolume(0.8);
  EXPECT_EQ(0.8, renderer.Volume());
  renderer.SetMuted(true);
  EXPECT_EQ(true, renderer.Muted());
  EXPECT_EQ(0.8, renderer.Volume());
  renderer.SetVolume(0.2);
  EXPECT_EQ(true, renderer.Muted());
  EXPECT_EQ(0.2, renderer.Volume());
  renderer.SetMuted(false);
  EXPECT_EQ(false, renderer.Muted());

  renderer.Attach(&stream);
  WAIT_WITH_TIMEOUT(did_init);
}

TEST_F(AudioRendererCommonTest, ReadsFrames) {
  auto info = MakeStreamInfo();
  stream.AddFrame(MakeFrame(info, 0, kData1));
  stream.AddFrame(MakeFrame(info, 2, kData1));
  stream.AddFrame(MakeFrame(info, 4, kData1));
  stream.AddFrame(MakeFrame(info, 6, kData2));
  stream.AddFrame(MakeFrame(info, 8, kData3));

  EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(6));

  ThreadEvent<void> did_append("");
  {
    InSequence seq;
    EXPECT_CALL(renderer, AppendBuffer(kData2, sizeof(kData2))).Times(1);
    EXPECT_CALL(renderer, AppendBuffer(kData3, sizeof(kData3)))
        .WillOnce(SignalAndReturn(did_append, true));
  }

  renderer.Attach(&stream);
  WAIT_WITH_TIMEOUT(did_append);
}

TEST_F(AudioRendererCommonTest, StopsOnError) {
  auto info = MakeStreamInfo();
  stream.AddFrame(MakeFrame(info, 0, kData1));
  stream.AddFrame(MakeFrame(info, 2, kData1));
  stream.AddFrame(MakeFrame(info, 4, kData1));

  {
    InSequence seq;
    EXPECT_CALL(renderer, AppendBuffer(kData1, sizeof(kData1)))
        .WillOnce(Return(false));
  }

  renderer.Attach(&stream);
  // Sleep to ensure we don't get any more calls.
  util::Clock::Instance.SleepSeconds(0.1);
}

TEST_F(AudioRendererCommonTest, StopsAfterEnoughBuffered) {
  auto info = MakeStreamInfo();
  stream.AddFrame(MakeFrame(info, 0, kData1));
  stream.AddFrame(MakeFrame(info, 2, kData2));
  stream.AddFrame(MakeFrame(info, 4, kData3));
  stream.AddFrame(MakeFrame(info, 6, kData1));
  stream.AddFrame(MakeFrame(info, 8, kData1));

  ThreadEvent<void> did_append("");
  {
    InSequence seq;
    EXPECT_CALL(renderer, GetBytesBuffered()).WillRepeatedly(Return(0));
    EXPECT_CALL(renderer, AppendBuffer(kData1, sizeof(kData1))).Times(1);
    EXPECT_CALL(renderer, GetBytesBuffered()).WillRepeatedly(Return(1));
    EXPECT_CALL(renderer, AppendBuffer(kData2, sizeof(kData2))).Times(1);
    // Simulate playing data, allow loop to poll a few times.
    EXPECT_CALL(renderer, GetBytesBuffered())
        .Times(3)
        .WillRepeatedly(Return(5));
    EXPECT_CALL(renderer, GetBytesBuffered()).WillRepeatedly(Return(2));
    EXPECT_CALL(renderer, AppendBuffer(kData3, sizeof(kData3)))
        .WillOnce(SignalAndReturn(did_append, true));
    EXPECT_CALL(renderer, GetBytesBuffered()).WillRepeatedly(Return(6));
  }

  renderer.Attach(&stream);
  WAIT_WITH_TIMEOUT(did_append);
}

TEST_F(AudioRendererCommonTest, InjectsSilenceBetweenFrames) {
  auto info = MakeStreamInfo();
  stream.AddFrame(MakeFrame(info, 0, kData1));
  stream.AddFrame(MakeFrame(info, 3, kData2));

  ThreadEvent<void> did_append("");
  {
    InSequence seq;
    EXPECT_CALL(renderer, AppendBuffer(kData1, sizeof(kData1))).Times(1);
    // One second of silence.
    EXPECT_CALL(renderer, AppendBuffer(_, 2)).Times(1);
    EXPECT_CALL(renderer, AppendBuffer(kData2, sizeof(kData2)))
        .WillOnce(SignalAndReturn(did_append, true));
  }

  renderer.Attach(&stream);
  WAIT_WITH_TIMEOUT(did_append);
}

TEST_F(AudioRendererCommonTest, SkipsOverlappingData) {
  auto info = MakeStreamInfo();
  stream.AddFrame(MakeFrame(info, 0, kData1));
  stream.AddFrame(MakeFrame(info, 1, kData2));
  stream.AddFrame(MakeFrame(info, 3, kData3));

  ThreadEvent<void> did_append("");
  {
    InSequence seq;
    EXPECT_CALL(renderer, AppendBuffer(kData1, sizeof(kData1))).Times(1);
    EXPECT_CALL(renderer, AppendBuffer(kData2 + 2, 2)).Times(1);
    EXPECT_CALL(renderer, AppendBuffer(kData3, sizeof(kData3)))
        .WillOnce(SignalAndReturn(did_append, true));
  }

  renderer.Attach(&stream);
  WAIT_WITH_TIMEOUT(did_append);
}

TEST_F(AudioRendererCommonTest, HandlesPlanarFormats) {
  // 2 Channels, 4 bytes-per-sample, 3 samples.  Use different values for each
  // to make sure the loop indexes are correct and not using a different value.
  // [Channel#][Sample#][Byte#]
  const uint8_t data1[] = {111, 112, 113, 114, 121, 122,
                           123, 124, 131, 132, 133, 134};
  const uint8_t data2[] = {211, 212, 213, 214, 221, 222,
                           223, 224, 231, 232, 233, 234};
  const uint8_t expected[] = {111, 112, 113, 114, 211, 212, 213, 214,
                              121, 122, 123, 124, 221, 222, 223, 224,
                              131, 132, 133, 134, 231, 232, 233, 234};
  std::shared_ptr<StreamInfo> info(
      new StreamInfo("", "", false, {0, 0}, {0, 0}, {}, 0, 0, 2, kSampleRate));
  std::shared_ptr<DecodedFrame> frame(
      new DecodedFrame(info, 0, 0, 0.01, SampleFormat::PlanarS32, 0,
                       {data1, data2}, {sizeof(data1), sizeof(data2)}));
  stream.AddFrame(frame);

  ThreadEvent<void> did_append("");
  {
    InSequence seq;
    EXPECT_CALL(renderer, AppendBuffer(_, sizeof(expected)))
        .With(Args<0, 1>(MatchesData(expected)))
        .WillOnce(SignalAndReturn(did_append, true));
  }

  renderer.Attach(&stream);
  WAIT_WITH_TIMEOUT(did_append);
}

TEST_F(AudioRendererCommonTest, ResetsDeviceForNewStream) {
  auto info1 = MakeStreamInfo();
  auto info2 = MakeStreamInfo();
  auto frame1 = MakeFrame(info1, 0, kData1);
  auto frame2 = MakeFrame(info2, 2, kData2);
  stream.AddFrame(frame1);
  stream.AddFrame(frame2);

  ThreadEvent<void> did_append("");
  {
    InSequence seq;
    EXPECT_CALL(player, CurrentTime()).WillOnce(Return(0));
    EXPECT_CALL(renderer, InitDevice(frame1, 1)).Times(1);
    EXPECT_CALL(renderer, AppendBuffer(kData1, sizeof(kData1))).Times(1);
    // After a reset, the buffer is empty, so it should use the current time to
    // synchronize the new frame.
    EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(1));
    EXPECT_CALL(renderer, InitDevice(frame2, 1)).Times(1);
    EXPECT_CALL(renderer, AppendBuffer(_, 2)).Times(1);  // Silence
    EXPECT_CALL(renderer, AppendBuffer(kData2, sizeof(kData2)))
        .WillOnce(SignalAndReturn(did_append, true));
    EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(4));
  }

  renderer.Attach(&stream);
  WAIT_WITH_TIMEOUT(did_append);
}

TEST_F(AudioRendererCommonTest, HandlesSeeks) {
  auto info = MakeStreamInfo();
  stream.AddFrame(MakeFrame(info, 0, kData1));
  stream.AddFrame(MakeFrame(info, 2, kData1));
  stream.AddFrame(MakeFrame(info, 4, kData1));
  stream.AddFrame(MakeFrame(info, 10, kData2));
  stream.AddFrame(MakeFrame(info, 12, kData2));
  stream.AddFrame(MakeFrame(info, 14, kData2));

  double time = 0;
  size_t buffered = 0;
  ThreadEvent<void> on_done("");
  ON_CALL(player, CurrentTime()).WillByDefault(ReturnPointee(&time));
  ON_CALL(renderer, GetBytesBuffered()).WillByDefault(ReturnPointee(&buffered));
  {
    InSequence seq;
    // Buffer the first segment, then wait since we're fully buffered.
    EXPECT_CALL(renderer, ClearBuffer()).Times(1);  // Clear during startup.
    EXPECT_CALL(renderer, AppendBuffer(kData1, sizeof(kData1)))
        .WillOnce(InvokeWithoutArgs([&]() {
          buffered = 2;  // Not enough buffered, will get another append.
          return true;
        }))
        .WillOnce(InvokeWithoutArgs([&]() {
          buffered = 6;  // Enough buffered, waits.
          return true;
        }));
    // Once waiting, perform the seek.
    EXPECT_CALL(clock, SleepSeconds(_)).WillOnce(InvokeWithoutArgs([&]() {
      time = 10;
      player_client->OnSeeking();
    }));

    // Clear buffer and buffer at the new time.
    EXPECT_CALL(renderer, ClearBuffer()).WillOnce(InvokeWithoutArgs([&]() {
      buffered = 0;
    }));
    EXPECT_CALL(renderer, AppendBuffer(kData2, sizeof(kData2)))
        .WillOnce(InvokeWithoutArgs([&]() {
          buffered = 2;
          return true;
        }))
        .WillOnce(InvokeWithoutArgs([&]() {
          buffered = 6;
          return true;
        }));
    // Once waiting, perform another seek
    EXPECT_CALL(clock, SleepSeconds(_)).WillOnce(InvokeWithoutArgs([&]() {
      time = 11;
      player_client->OnSeeking();
    }));

    // Clear buffer and buffer at the new time.
    EXPECT_CALL(renderer, ClearBuffer()).WillOnce(InvokeWithoutArgs([&]() {
      buffered = 0;
    }));
    // Partial frame since we're inside the frame.
    EXPECT_CALL(renderer, AppendBuffer(kData2 + 2, 2))
        .WillOnce(InvokeWithoutArgs([&]() {
          buffered = 2;
          return true;
        }));
    EXPECT_CALL(renderer, AppendBuffer(kData2, sizeof(kData2)))
        .WillOnce(InvokeWithoutArgs([&]() {
          buffered = 6;
          return true;
        }));
    EXPECT_CALL(clock, SleepSeconds(_)).WillOnce(InvokeWithoutArgs([&]() {
      on_done.SignalAll();
      renderer.Stop();
    }));
  }

  renderer.Attach(&stream);
  WAIT_WITH_TIMEOUT(on_done);
}

}  // namespace media
}  // namespace shaka
