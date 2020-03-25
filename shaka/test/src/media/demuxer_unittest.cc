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

#include "shaka/media/demuxer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/media/media_utils.h"
#include "src/test/media_files.h"
#include "src/util/crypto.h"
#include "test/src/media/media_tests.pb.h"

namespace shaka {
namespace media {

namespace {

using testing::_;
using testing::NiceMock;

std::string GetFrameHash(const uint8_t* data, const int size) {
  const std::vector<uint8_t> digest = util::HashData(data, size);
  return util::ToHexString(digest.data(), digest.size());
}

class MockClient : public Demuxer::Client {
 public:
  MOCK_METHOD1(OnLoadedMetaData, void(double));
  MOCK_METHOD3(OnEncrypted,
               void(eme::MediaKeyInitDataType, const uint8_t*, size_t));
};

void RunDemuxerTest(const std::vector<std::string>& files) {
  NiceMock<MockClient> client;
  std::unique_ptr<Demuxer> demuxer;

  EXPECT_CALL(client, OnLoadedMetaData(_)).Times(1);

  for (const std::string& file : files) {
    const std::vector<uint8_t> proto_data = GetMediaFile(file + ".dat");
    proto::MediaInfo info;
    ASSERT_TRUE(info.ParseFromArray(proto_data.data(), proto_data.size()));

    if (!demuxer) {
      demuxer = DemuxerFactory::GetFactory()->Create(info.mime(), &client);
      ASSERT_TRUE(demuxer);
    }

    const std::vector<uint8_t> media_data = GetMediaFile(file);
    std::vector<std::shared_ptr<EncodedFrame>> frames;
    ASSERT_TRUE(
        demuxer->Demux(0, media_data.data(), media_data.size(), &frames));
    ASSERT_EQ(frames.size(), info.frames().size());
    if (!frames.empty()) {
      // All frames for the same input file should have the same stream object.
      std::shared_ptr<const StreamInfo> stream = frames[0]->stream_info;
      ASSERT_TRUE(stream);
      EXPECT_EQ(stream->mime_type, info.mime());
      EXPECT_EQ(stream->time_scale.numerator, info.stream().time_scale_num());
      EXPECT_EQ(stream->time_scale.denominator, info.stream().time_scale_den());
      EXPECT_EQ(stream->is_video, info.stream().is_video());
      const std::string extra_data_hash =
          GetFrameHash(stream->extra_data.data(), stream->extra_data.size());
      EXPECT_EQ(extra_data_hash, info.stream().extra_data_hash());

      for (size_t i = 0; i < frames.size(); i++) {
        const auto& expected = info.frames().at(i);
        EXPECT_EQ(frames[i]->stream_info, stream);
        EXPECT_EQ(!!frames[i]->encryption_info, expected.is_encrypted());
        EXPECT_NEAR(frames[i]->pts, expected.pts(), 0.0001);
        EXPECT_NEAR(frames[i]->dts, expected.dts(), 0.0001);
        if (expected.has_duration())
          EXPECT_NEAR(frames[i]->duration, expected.duration(), 0.0001);

        const std::string hash =
            GetFrameHash(frames[i]->data, frames[i]->data_size);
        EXPECT_EQ(hash, expected.data_hash());
      }
    }
  }
}

}  // namespace

TEST(DemuxerTest, SingleFile) {
  RunDemuxerTest({"clear_high.mp4"});
}

TEST(DemuxerTest, Segmented) {
  RunDemuxerTest({"clear_low_frag_init.mp4", "clear_low_frag_seg1.mp4"});
}

TEST(DemuxerTest, SegmentedWithAdaptation) {
  RunDemuxerTest(
      {"clear_low_frag_init.mp4", "clear_low_frag_seg1.mp4", "clear_high.mp4"});
}

TEST(DemuxerTest, Encrypted) {
  RunDemuxerTest({"encrypted_low.mp4"});
}

}  // namespace media
}  // namespace shaka
