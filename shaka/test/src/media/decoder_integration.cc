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

extern "C" {
#include <libavutil/imgutils.h>
}

#include "shaka/media/decoder.h"
#include "shaka/media/demuxer.h"
#include "shaka/media/frames.h"
#include "src/eme/clearkey_implementation.h"
#include "src/media/ffmpeg/ffmpeg_decoded_frame.h"
#include "src/media/ffmpeg/ffmpeg_decoder.h"
#include "src/media/media_utils.h"
#include "src/test/frame_converter.h"
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

constexpr const char* kHashFile = "hash_file.txt";

class DemuxerClient : public Demuxer::Client {
 public:
  MOCK_METHOD1(OnLoadedMetaData, void(double));
  MOCK_METHOD3(OnEncrypted,
               void(eme::MediaKeyInitDataType, const uint8_t*, size_t));
};

bool EndsWith(const std::string& value, const std::string& sub) {
  return sub.size() <= value.size() &&
         value.substr(value.size() - sub.size()) == sub;
}

std::string GetFrameHash(const uint8_t* data, const int size) {
  const std::vector<uint8_t> digest = util::HashData(data, size);
  return util::ToHexString(digest.data(), digest.size());
}

void DemuxFiles(const std::vector<std::string>& paths,
                std::vector<std::shared_ptr<EncodedFrame>>* frames) {
  NiceMock<DemuxerClient> client;
  auto* factory = DemuxerFactory::GetFactory();
  ASSERT_TRUE(factory);
  const std::string mime =
      EndsWith(paths[0], ".webm") ? "video/webm" : "video/mp4";
  auto demuxer = factory->Create(mime, &client);
  ASSERT_TRUE(demuxer);

  for (const auto& path : paths) {
    std::vector<uint8_t> data = GetMediaFile(path);
    ASSERT_TRUE(demuxer->Demux(0, data.data(), data.size(), frames));
  }
}

void DecodeFramesAndCheckHashes(
    const std::vector<std::shared_ptr<EncodedFrame>>& input_frames,
    Decoder* decoder, eme::Implementation* cdm) {
  FrameConverter converter;
  std::string results;
  // Run this loop one extra time to pass "nullptr" to flush the last frames.
  for (size_t i = 0; i <= input_frames.size(); i++) {
    auto frame = i < input_frames.size() ? input_frames[i] : nullptr;
    std::vector<std::shared_ptr<DecodedFrame>> decoded_frames;
    ASSERT_EQ(decoder->Decode(frame, cdm, &decoded_frames),
              MediaStatus::Success);

    for (auto& decoded : decoded_frames) {
      const uint8_t* data;
      size_t size;
      ASSERT_TRUE(converter.ConvertFrame(decoded, &data, &size));
      results += GetFrameHash(data, size) + "\n";
    }
  }

  const std::vector<uint8_t> expected = GetMediaFile(kHashFile);
  EXPECT_EQ(results, std::string(expected.begin(), expected.end()));
}

std::unique_ptr<Decoder> MakeDecoder() {
  return std::unique_ptr<Decoder>(new ffmpeg::FFmpegDecoder);
}

}  // namespace

class DecoderIntegration : public testing::Test {
 protected:
  void LoadKeyForTesting(eme::ClearKeyImplementation* clear_key,
                         const std::vector<uint8_t>& key_id,
                         const std::vector<uint8_t>& key) {
    clear_key->LoadKeyForTesting(key_id, key);
  }
};

TEST_F(DecoderIntegration, CanDecodeFrames) {
  std::vector<std::shared_ptr<EncodedFrame>> frames;
  ASSERT_NO_FATAL_FAILURE(DemuxFiles({kMp4LowInit, kMp4LowSeg}, &frames));

  auto decoder = MakeDecoder();
  DecodeFramesAndCheckHashes(frames, decoder.get(), nullptr);
}

TEST_F(DecoderIntegration, CanDecodeWithAdaptation) {
  std::vector<std::shared_ptr<EncodedFrame>> frames;
  ASSERT_NO_FATAL_FAILURE(
      DemuxFiles({kMp4LowInit, kMp4LowSeg, kMp4High}, &frames));

  auto decoder = MakeDecoder();

  bool saw_first_stream = false;
  bool saw_second_stream = false;
  std::shared_ptr<const StreamInfo> first_stream_info;
  for (const auto& frame : frames) {
    if (!saw_first_stream) {
      saw_first_stream = true;
      first_stream_info = frame->stream_info;
    } else {
      if (frame->stream_info != first_stream_info)
        saw_second_stream = true;
    }

    std::vector<std::shared_ptr<DecodedFrame>> decoded_frames;
    ASSERT_EQ(decoder->Decode(frame, nullptr, &decoded_frames),
              MediaStatus::Success);
  }

  EXPECT_TRUE(saw_first_stream);
  EXPECT_TRUE(saw_second_stream);
}


class DecoderDecryptIntegration : public testing::TestWithParam<const char*> {
 protected:
  DecoderDecryptIntegration() : cdm_(nullptr) {
    cdm_.LoadKeyForTesting({0xab, 0xba, 0x27, 0x1e, 0x8b, 0xcf, 0x55, 0x2b,
                            0xbd, 0x2e, 0x86, 0xa4, 0x34, 0xa9, 0xa5, 0xd9},
                           {0x69, 0xea, 0xa8, 0x02, 0xa6, 0x76, 0x3a, 0xf9,
                            0x79, 0xe8, 0xd1, 0x94, 0x0f, 0xb8, 0x83, 0x92});
  }

  eme::ClearKeyImplementation cdm_;
};

TEST_P(DecoderDecryptIntegration, CanDecryptFrames) {
  const std::string container = EndsWith(GetParam(), ".webm") ? "webm" : "mp4";
  auto* factory = DemuxerFactory::GetFactory();
  if (!factory || !factory->IsTypeSupported(container)) {
    LOG(WARNING) << "Skipping test since we don't have support for the media.";
    return;
  }

  std::vector<std::shared_ptr<EncodedFrame>> frames;
  ASSERT_NO_FATAL_FAILURE(DemuxFiles({GetParam()}, &frames));

  auto decoder = MakeDecoder();
  DecodeFramesAndCheckHashes(frames, decoder.get(), &cdm_);
}

INSTANTIATE_TEST_CASE_P(SupportsNormalCase, DecoderDecryptIntegration,
                        testing::Values("encrypted_low.mp4",
                                        "encrypted_low.webm"));

INSTANTIATE_TEST_CASE_P(SupportsUnusualCases, DecoderDecryptIntegration,
                        testing::Values("encrypted_low_cenc.mp4",
                                        "encrypted_low_cens.mp4",
                                        "encrypted_low_cbc1.mp4",
                                        "encrypted_low_cbcs.mp4"));

}  // namespace media
}  // namespace shaka
