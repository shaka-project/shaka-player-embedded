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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/media/media_processor.h"

extern "C" {
#include <libavutil/imgutils.h>
}

#include "src/eme/clearkey_implementation.h"
#include "src/media/ffmpeg/ffmpeg_decoded_frame.h"
#include "src/media/ffmpeg/ffmpeg_encoded_frame.h"
#include "src/media/frame_converter.h"
#include "src/media/media_utils.h"
#include "src/test/media_files.h"
#include "src/util/crypto.h"

namespace shaka {
namespace media {

namespace {

using namespace std::placeholders;  // NOLINT
using testing::_;
using testing::Gt;
using testing::Invoke;
using testing::MockFunction;
using testing::NiceMock;
using testing::ReturnPointee;

constexpr const char* kMp4LowInit = "clear_low_frag_init.mp4";
constexpr const char* kMp4LowSeg = "clear_low_frag_seg1.mp4";
// This isn't fragmented, so it doesn't need an explicit init segment.
constexpr const char* kMp4High = "clear_high.mp4";
constexpr const char* kMp4Encrypted = "encrypted_low.mp4";
constexpr const char* kMp4KeyRotation = "encrypted_key_rotation.mp4";

constexpr const char* kHashFile = "hash_file.txt";

/**
 * A simple helper that reads from one or more buffers into the MediaProcessor.
 */
class SegmentReader {
 public:
  SegmentReader() : segment_idx_(0), segment_offset_(0) {}

  void AppendSegment(std::vector<uint8_t> buffer) {
    segments_.emplace_back(std::move(buffer));
  }

  std::function<size_t(uint8_t*, size_t)> MakeReadCallback() {
    return [this](uint8_t* dest, size_t dest_size) -> size_t {
      if (segment_idx_ >= segments_.size())
        return 0;

      std::vector<uint8_t>* segment = &segments_[segment_idx_];
      dest_size = std::min(dest_size, segment->size() - segment_offset_);
      memcpy(dest, segment->data() + segment_offset_, dest_size);
      segment_offset_ += dest_size;

      if (segment_offset_ >= segment->size()) {
        segment_idx_++;
        segment_offset_ = 0;
      }

      return dest_size;
    };
  }

  std::function<void()> MakeResetReadCallback() {
    return [this]() { segment_offset_ = 0; };
  }

 private:
  std::vector<std::vector<uint8_t>> segments_;
  size_t segment_idx_;
  size_t segment_offset_;
};

std::string GetFrameHash(const uint8_t* data, const int size) {
  const std::vector<uint8_t> digest = util::HashData(data, size);
  return util::ToHexString(digest.data(), digest.size());
}

void DecodeFramesAndCheckHashes(MediaProcessor* processor,
                                eme::Implementation* cdm) {
  FrameConverter converter;
  std::string results;
  Status status = Status::Success;
  while (status != Status::EndOfStream) {
    std::shared_ptr<EncodedFrame> frame;
    status = processor->ReadDemuxedFrame(&frame);
    if (status != Status::EndOfStream)
      ASSERT_EQ(status, Status::Success);

    std::vector<std::shared_ptr<DecodedFrame>> decoded_frames;
    ASSERT_EQ(processor->DecodeFrame(0, frame, cdm, &decoded_frames),
              Status::Success);
    for (auto& decoded : decoded_frames) {
      auto* cast_frame =
          static_cast<ffmpeg::FFmpegDecodedFrame*>(decoded.get());
      const uint8_t* const* data = cast_frame->data.data();
      std::vector<int> linesize_vec{cast_frame->linesize.begin(),
                                    cast_frame->linesize.end()};
      const int* linesize = linesize_vec.data();
      if (cast_frame->raw_frame()->format != AV_PIX_FMT_ARGB) {
        ASSERT_TRUE(converter.ConvertFrame(cast_frame->raw_frame(), &data,
                                           &linesize, AV_PIX_FMT_ARGB));
      }

      results += GetFrameHash(data[0], linesize[0] * cast_frame->height) + "\n";
    }
  }

  const std::vector<uint8_t> expected = GetMediaFile(kHashFile);
  EXPECT_EQ(results, std::string(expected.begin(), expected.end()));
}

void ExpectNoAdaptation() {
  EXPECT_FALSE(true) << "Not expecting adaptation.";
}

void IgnoreInitData(eme::MediaKeyInitDataType, const uint8_t*, size_t) {}

}  // namespace

class MediaProcessorIntegration : public testing::Test {
 protected:
  void LoadKeyForTesting(eme::ClearKeyImplementation* clear_key,
                         const std::vector<uint8_t>& key_id,
                         const std::vector<uint8_t>& key) {
    clear_key->LoadKeyForTesting(key_id, key);
  }
};

TEST_F(MediaProcessorIntegration, ReadsInitSegment) {
  SegmentReader reader;
  reader.AppendSegment(GetMediaFile(kMp4LowInit));


  MediaProcessor::Initialize();
  MediaProcessor processor("mp4", "avc1.42c01e", &IgnoreInitData);
  ASSERT_EQ(processor.InitializeDemuxer(reader.MakeReadCallback(),
                                        &ExpectNoAdaptation),
            Status::Success);
}

TEST_F(MediaProcessorIntegration, ReadsDemuxedFrames) {
  SegmentReader reader;
  reader.AppendSegment(GetMediaFile(kMp4LowInit));
  reader.AppendSegment(GetMediaFile(kMp4LowSeg));


  MediaProcessor::Initialize();
  MediaProcessor processor("mp4", "avc1.42c01e", &IgnoreInitData);
  ASSERT_EQ(processor.InitializeDemuxer(reader.MakeReadCallback(),
                                        &ExpectNoAdaptation),
            Status::Success);

  for (size_t i = 0; i < 120; i++) {
    std::shared_ptr<EncodedFrame> frame;
    ASSERT_EQ(processor.ReadDemuxedFrame(&frame), Status::Success);
    // Don't test the body of the frame, it will be tested by the decoding test
    // below.
    EXPECT_NEAR(frame->dts, i * 0.041666, 0.0001);
  }

  std::shared_ptr<EncodedFrame> frame;
  ASSERT_EQ(processor.ReadDemuxedFrame(&frame), Status::EndOfStream);
}

TEST_F(MediaProcessorIntegration, HandlesMp4Adaptation) {
  SegmentReader reader;
  reader.AppendSegment(GetMediaFile(kMp4LowInit));
  reader.AppendSegment(GetMediaFile(kMp4LowSeg));
  reader.AppendSegment(GetMediaFile(kMp4High));


  MediaProcessor::Initialize();
  MediaProcessor processor("mp4", "avc1.42c01e", &IgnoreInitData);
  ASSERT_EQ(processor.InitializeDemuxer(reader.MakeReadCallback(),
                                        reader.MakeResetReadCallback()),
            Status::Success);

  // Low segment.
  for (size_t i = 0; i < 120; i++) {
    std::shared_ptr<EncodedFrame> frame;
    ASSERT_EQ(processor.ReadDemuxedFrame(&frame), Status::Success);
    EXPECT_NEAR(frame->dts, i * 0.041666, 0.0001);
  }

  // High segment.
  for (size_t i = 0; i < 120; i++) {
    std::shared_ptr<EncodedFrame> frame;
    ASSERT_EQ(processor.ReadDemuxedFrame(&frame), Status::Success);
    // The high segment also starts at 0.
    EXPECT_NEAR(frame->dts, i * 0.041666, 0.0001);
  }

  std::shared_ptr<EncodedFrame> frame;
  ASSERT_EQ(processor.ReadDemuxedFrame(&frame), Status::EndOfStream);
}

TEST_F(MediaProcessorIntegration, AccountsForTimestampOffset) {
  SegmentReader reader;
  reader.AppendSegment(GetMediaFile(kMp4LowInit));
  reader.AppendSegment(GetMediaFile(kMp4LowSeg));


  MediaProcessor::Initialize();
  MediaProcessor processor("mp4", "avc1.42c01e", &IgnoreInitData);
  processor.SetTimestampOffset(20);
  ASSERT_EQ(processor.InitializeDemuxer(reader.MakeReadCallback(),
                                        &ExpectNoAdaptation),
            Status::Success);

  std::shared_ptr<EncodedFrame> frame;
  ASSERT_EQ(processor.ReadDemuxedFrame(&frame), Status::Success);
  EXPECT_NEAR(frame->dts, 20, 0.0001);
  EXPECT_NEAR(frame->pts, 20, 0.0001);

  ASSERT_EQ(processor.ReadDemuxedFrame(&frame), Status::Success);
  EXPECT_NEAR(frame->dts, 20.041666, 0.0001);
  EXPECT_NEAR(frame->pts, 20.041666, 0.0001);
}

TEST_F(MediaProcessorIntegration, DemuxerReportsEncryptedFrames) {
  SegmentReader reader;
  reader.AppendSegment(GetMediaFile(kMp4Encrypted));


  MediaProcessor::Initialize();
  MediaProcessor processor("mp4", "avc1.42c01e", &IgnoreInitData);
  ASSERT_EQ(processor.InitializeDemuxer(reader.MakeReadCallback(),
                                        &ExpectNoAdaptation),
            Status::Success);

  for (size_t i = 0; i < 120; i++) {
    std::shared_ptr<EncodedFrame> frame;
    ASSERT_EQ(processor.ReadDemuxedFrame(&frame), Status::Success);
    // The first 96 frames are clear, until the second keyframe.
    EXPECT_EQ(frame->is_encrypted, i >= 96);
  }
}

TEST_F(MediaProcessorIntegration, ReportsEncryptionInitInfo) {
  using namespace std::placeholders;

  SegmentReader reader;
  reader.AppendSegment(GetMediaFile(kMp4KeyRotation));

  MockFunction<void(eme::MediaKeyInitDataType, const uint8_t*, size_t)>
      on_init_info;

  EXPECT_CALL(on_init_info, Call(_, _, _)).Times(0);
  // There should be two media segments each with 2 'pssh' boxes.  This should
  // combine the 'pssh' boxes that appear in the same segment.
  EXPECT_CALL(on_init_info, Call(eme::MediaKeyInitDataType::Cenc, _, Gt(0u)))
      .Times(2);

  auto cb = std::bind(&decltype(on_init_info)::Call, &on_init_info, _1, _2, _3);
  MediaProcessor::Initialize();
  MediaProcessor processor("mp4", "avc1.42c01e", cb);
  ASSERT_EQ(processor.InitializeDemuxer(reader.MakeReadCallback(),
                                        &ExpectNoAdaptation),
            Status::Success);

  for (size_t i = 0; i < 120; i++) {
    std::shared_ptr<EncodedFrame> frame;
    ASSERT_EQ(processor.ReadDemuxedFrame(&frame), Status::Success);
  }

  std::shared_ptr<EncodedFrame> frame;
  ASSERT_EQ(processor.ReadDemuxedFrame(&frame), Status::EndOfStream);
}

TEST_F(MediaProcessorIntegration, CanDecodeFrames) {
  SegmentReader reader;
  reader.AppendSegment(GetMediaFile(kMp4LowInit));
  reader.AppendSegment(GetMediaFile(kMp4LowSeg));


  MediaProcessor::Initialize();
  MediaProcessor processor("mp4", "avc1.42c01e", &IgnoreInitData);
  ASSERT_EQ(processor.InitializeDemuxer(reader.MakeReadCallback(),
                                        &ExpectNoAdaptation),
            Status::Success);

  DecodeFramesAndCheckHashes(&processor, nullptr);
}

TEST_F(MediaProcessorIntegration, CanDecodeWithAdaptation) {
  SegmentReader reader;
  reader.AppendSegment(GetMediaFile(kMp4LowInit));
  reader.AppendSegment(GetMediaFile(kMp4LowSeg));
  reader.AppendSegment(GetMediaFile(kMp4High));


  MediaProcessor::Initialize();
  MediaProcessor processor("mp4", "avc1.42c01e", &IgnoreInitData);
  ASSERT_EQ(processor.InitializeDemuxer(reader.MakeReadCallback(),
                                        reader.MakeResetReadCallback()),
            Status::Success);

  bool saw_first_stream = false;
  bool saw_second_stream = false;
  size_t first_stream_id = -1;
  while (true) {
    std::shared_ptr<EncodedFrame> frame;
    const Status status = processor.ReadDemuxedFrame(&frame);
    if (status == Status::EndOfStream)
      break;

    auto* ffmpeg_frame = static_cast<ffmpeg::FFmpegEncodedFrame*>(frame.get());
    if (!saw_first_stream) {
      saw_first_stream = true;
      first_stream_id = ffmpeg_frame->stream_id();
    } else {
      if (ffmpeg_frame->stream_id() != first_stream_id)
        saw_second_stream = true;
    }

    std::vector<std::shared_ptr<DecodedFrame>> decoded_frames;
    ASSERT_EQ(status, Status::Success);
    ASSERT_EQ(processor.DecodeFrame(0, frame, nullptr, &decoded_frames),
              Status::Success);
  }

  EXPECT_TRUE(saw_first_stream);
  EXPECT_TRUE(saw_second_stream);
}


class MediaProcessorDecryptIntegration
    : public testing::TestWithParam<const char*> {
 protected:
  MediaProcessorDecryptIntegration() : cdm_(nullptr) {
    cdm_.LoadKeyForTesting({0xab, 0xba, 0x27, 0x1e, 0x8b, 0xcf, 0x55, 0x2b,
                            0xbd, 0x2e, 0x86, 0xa4, 0x34, 0xa9, 0xa5, 0xd9},
                           {0x69, 0xea, 0xa8, 0x02, 0xa6, 0x76, 0x3a, 0xf9,
                            0x79, 0xe8, 0xd1, 0x94, 0x0f, 0xb8, 0x83, 0x92});
  }

  eme::ClearKeyImplementation cdm_;
};

TEST_P(MediaProcessorDecryptIntegration, CanDecryptFrames) {
  SegmentReader reader;
  reader.AppendSegment(GetMediaFile(GetParam()));

  auto ends_with = [this](const std::string& sub) {
    const std::string param = GetParam();
    return sub.size() <= param.size() &&
           param.substr(param.size() - sub.size()) == sub;
  };

  MediaProcessor::Initialize();
  const std::string container = ends_with(".webm") ? "webm" : "mp4";
  const std::string codec = ends_with(".webm") ? "vp9" : "avc1.42c01e";
  if (!IsTypeSupported(container, codec, 0, 0)) {
    LOG(WARNING) << "Skipping test since we don't have support for the media.";
    return;
  }

  MediaProcessor processor(container, codec, &IgnoreInitData);
  ASSERT_EQ(processor.InitializeDemuxer(reader.MakeReadCallback(),
                                        &ExpectNoAdaptation),
            Status::Success);

  DecodeFramesAndCheckHashes(&processor, &cdm_);
}

INSTANTIATE_TEST_CASE_P(SupportsNormalCase, MediaProcessorDecryptIntegration,
                        testing::Values("encrypted_low.mp4",
                                        "encrypted_low.webm"));

INSTANTIATE_TEST_CASE_P(SupportsUnusualCases, MediaProcessorDecryptIntegration,
                        testing::Values("encrypted_low_cenc.mp4",
                                        "encrypted_low_cens.mp4",
                                        "encrypted_low_cbc1.mp4",
                                        "encrypted_low_cbcs.mp4"));

}  // namespace media
}  // namespace shaka
