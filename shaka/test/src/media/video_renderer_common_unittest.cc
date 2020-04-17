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

#include "src/media/video_renderer_common.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "shaka/media/frames.h"
#include "shaka/media/streams.h"

namespace shaka {
namespace media {

namespace {

using testing::_;
using testing::InSequence;
using testing::MockFunction;
using testing::Return;
using testing::SaveArg;

constexpr const double kMinDelay = 1.0 / 120;

std::shared_ptr<DecodedFrame> MakeFrame(double start) {
  auto* ret = new DecodedFrame(nullptr, start, start, 0.01, PixelFormat::RGB24,
                               0, {}, {});
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

}  // namespace

TEST(VideoRendererCommonTest, WorksWithNoNextFrame) {
  DecodedStream stream;
  auto frame = MakeFrame(0.0);
  stream.AddFrame(frame);

  MockMediaPlayer player;
  EXPECT_CALL(player, PlaybackState())
      .WillRepeatedly(Return(VideoPlaybackState::Playing));
  EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(0));

  VideoRendererCommon renderer;
  renderer.SetPlayer(&player);
  renderer.Attach(&stream);


  std::shared_ptr<DecodedFrame> cur_frame;
  double delay = renderer.GetCurrentFrame(&cur_frame);
  EXPECT_EQ(cur_frame, frame);
  EXPECT_EQ(delay, kMinDelay);
}

TEST(VideoRendererCommonTest, WorksWithNoFrames) {
  DecodedStream stream;

  MockMediaPlayer player;
  EXPECT_CALL(player, PlaybackState())
      .WillRepeatedly(Return(VideoPlaybackState::Playing));
  EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(0));

  VideoRendererCommon renderer;
  renderer.SetPlayer(&player);
  renderer.Attach(&stream);

  std::shared_ptr<DecodedFrame> cur_frame;
  renderer.GetCurrentFrame(&cur_frame);
  EXPECT_FALSE(cur_frame);
}

TEST(VideoRendererCommonTest, DrawsFrameInPast) {
  DecodedStream stream;
  auto frame = MakeFrame(0.0);
  stream.AddFrame(frame);

  MockMediaPlayer player;
  EXPECT_CALL(player, PlaybackState())
      .WillRepeatedly(Return(VideoPlaybackState::Playing));
  EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(4));

  VideoRendererCommon renderer;
  renderer.SetPlayer(&player);
  renderer.Attach(&stream);

  std::shared_ptr<DecodedFrame> cur_frame;
  double delay = renderer.GetCurrentFrame(&cur_frame);
  EXPECT_EQ(cur_frame, frame);
  EXPECT_DOUBLE_EQ(delay, kMinDelay);
}

TEST(VideoRendererCommonTest, TracksDroppedFrames) {
  DecodedStream stream;
  stream.AddFrame(MakeFrame(0.00));
  stream.AddFrame(MakeFrame(0.01));
  stream.AddFrame(MakeFrame(0.02));
  stream.AddFrame(MakeFrame(0.03));
  stream.AddFrame(MakeFrame(0.04));

  MockMediaPlayer player;
  MockFunction<void()> step;
  EXPECT_CALL(player, PlaybackState())
      .WillRepeatedly(Return(VideoPlaybackState::Playing));

  {
    InSequence seq;
    EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(0));
    EXPECT_CALL(step, Call()).Times(1);
    EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(0.03));
  }

  VideoRendererCommon renderer;
  renderer.SetPlayer(&player);
  renderer.Attach(&stream);

  std::shared_ptr<DecodedFrame> cur_frame;

  // Time: 0
  double delay = renderer.GetCurrentFrame(&cur_frame);
  EXPECT_EQ(cur_frame, stream.GetFrame(0, FrameLocation::Near));
  EXPECT_EQ(renderer.VideoPlaybackQuality().dropped_video_frames, 0);
  EXPECT_DOUBLE_EQ(delay, 0.01);
  step.Call();

  // Time: 0.03
  delay = renderer.GetCurrentFrame(&cur_frame);
  EXPECT_EQ(cur_frame, stream.GetFrame(0.03, FrameLocation::Near));
  EXPECT_EQ(renderer.VideoPlaybackQuality().dropped_video_frames, 2);
  EXPECT_DOUBLE_EQ(delay, 0.01);
}

TEST(VideoRendererCommonTest, HandlesSeeks) {
  DecodedStream stream;
  stream.AddFrame(MakeFrame(0.00));
  stream.AddFrame(MakeFrame(0.01));
  stream.AddFrame(MakeFrame(0.02));
  stream.AddFrame(MakeFrame(0.03));
  stream.AddFrame(MakeFrame(0.04));

  MockMediaPlayer player;
  MockFunction<void()> step;
  MediaPlayer::Client* client;
  EXPECT_CALL(player, PlaybackState())
      .WillRepeatedly(Return(VideoPlaybackState::Playing));
  EXPECT_CALL(player, AddClient(_)).WillOnce(SaveArg<0>(&client));

  {
    InSequence seq;
    EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(0));
    EXPECT_CALL(step, Call()).Times(1);
    // Seek
    EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(0.03));
  }

  VideoRendererCommon renderer;
  renderer.SetPlayer(&player);
  renderer.Attach(&stream);

  std::shared_ptr<DecodedFrame> cur_frame;

  // Time: 0
  double delay = renderer.GetCurrentFrame(&cur_frame);
  EXPECT_EQ(cur_frame, stream.GetFrame(0, FrameLocation::Near));
  EXPECT_EQ(renderer.VideoPlaybackQuality().dropped_video_frames, 0);
  EXPECT_EQ(renderer.VideoPlaybackQuality().total_video_frames, 1);
  EXPECT_DOUBLE_EQ(delay, 0.01);

  client->OnSeeking();
  step.Call();

  // Time: 0.04
  delay = renderer.GetCurrentFrame(&cur_frame);
  EXPECT_EQ(cur_frame, stream.GetFrame(0.03, FrameLocation::Near));
  // Skipped over frames, but don't count.
  EXPECT_EQ(renderer.VideoPlaybackQuality().dropped_video_frames, 0);
  EXPECT_EQ(renderer.VideoPlaybackQuality().total_video_frames, 2);
  EXPECT_DOUBLE_EQ(delay, 0.01);
}

TEST(VideoRendererCommonTest, TracksNewFrames) {
  DecodedStream stream;
  stream.AddFrame(MakeFrame(0.00));
  stream.AddFrame(MakeFrame(0.02));
  stream.AddFrame(MakeFrame(0.04));

  MockFunction<void()> step;
  MockMediaPlayer player;
  EXPECT_CALL(player, PlaybackState())
      .WillRepeatedly(Return(VideoPlaybackState::Playing));

  {
    InSequence seq;
    EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(0));
    EXPECT_CALL(step, Call()).Times(1);
    EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(0.006));
    EXPECT_CALL(step, Call()).Times(1);
    EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(0.006));
    EXPECT_CALL(step, Call()).Times(1);
    EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(0.025));
    EXPECT_CALL(step, Call()).Times(1);
    EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(0.031));
    EXPECT_CALL(step, Call()).Times(1);
    EXPECT_CALL(player, CurrentTime()).WillRepeatedly(Return(0.044));
  }

  VideoRendererCommon renderer;
  renderer.SetPlayer(&player);
  renderer.Attach(&stream);

  std::shared_ptr<DecodedFrame> cur_frame;

#define FRAME_AT(i) (stream.GetFrame(i, FrameLocation::Near))

  // Time: 0
  double delay = renderer.GetCurrentFrame(&cur_frame);
  EXPECT_EQ(renderer.VideoPlaybackQuality().dropped_video_frames, 0);
  EXPECT_EQ(renderer.VideoPlaybackQuality().total_video_frames, 1);
  EXPECT_DOUBLE_EQ(delay, 0.02);
  step.Call();

  for (int i = 0; i < 2; i++) {
    // Time: 0.006
    delay = renderer.GetCurrentFrame(&cur_frame);
    EXPECT_EQ(cur_frame, FRAME_AT(0));
    EXPECT_EQ(renderer.VideoPlaybackQuality().dropped_video_frames, 0);
    EXPECT_EQ(renderer.VideoPlaybackQuality().total_video_frames, 1);
    EXPECT_DOUBLE_EQ(delay, 0.014);
    step.Call();
  }

  // Time: 0.025
  delay = renderer.GetCurrentFrame(&cur_frame);
  EXPECT_EQ(cur_frame, FRAME_AT(0.02));
  EXPECT_EQ(renderer.VideoPlaybackQuality().dropped_video_frames, 0);
  EXPECT_EQ(renderer.VideoPlaybackQuality().total_video_frames, 2);
  EXPECT_DOUBLE_EQ(delay, 0.015);
  step.Call();

  // Time: 0.031
  delay = renderer.GetCurrentFrame(&cur_frame);
  EXPECT_EQ(cur_frame, FRAME_AT(0.02));
  EXPECT_EQ(renderer.VideoPlaybackQuality().dropped_video_frames, 0);
  EXPECT_EQ(renderer.VideoPlaybackQuality().total_video_frames, 2);
  EXPECT_DOUBLE_EQ(delay, 0.009);
  step.Call();

  // Time: 0.044
  delay = renderer.GetCurrentFrame(&cur_frame);
  EXPECT_EQ(cur_frame, FRAME_AT(0.04));
  EXPECT_EQ(renderer.VideoPlaybackQuality().dropped_video_frames, 0);
  EXPECT_EQ(renderer.VideoPlaybackQuality().total_video_frames, 3);
  EXPECT_DOUBLE_EQ(delay, kMinDelay);
#undef FRAME_AT
}

}  // namespace media
}  // namespace shaka
