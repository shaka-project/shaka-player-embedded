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

#include "src/media/media_utils.h"

#include <gtest/gtest.h>

namespace shaka {
namespace media {

namespace {

void BadMimeTest(const std::string& source) {
  std::string type;
  std::string subtype;
  std::unordered_map<std::string, std::string> params;
  EXPECT_FALSE(ParseMimeType(source, &type, &subtype, &params));
}

void GoodMimeTest(const std::string& source) {
  std::string type;
  std::string subtype;
  std::unordered_map<std::string, std::string> params;
  EXPECT_TRUE(ParseMimeType(source, &type, &subtype, &params));
}

}  // namespace

TEST(MediaUtilsTest, ParseMimeType) {
  std::string type;
  std::string subtype;
  std::unordered_map<std::string, std::string> params;

  ASSERT_TRUE(ParseMimeType("video/mp4", &type, &subtype, &params));
  EXPECT_EQ("video", type);
  EXPECT_EQ("mp4", subtype);
  EXPECT_EQ(0u, params.size());

  params.clear();
  ASSERT_TRUE(ParseMimeType("audio/mp2t; codecs = \"foo bar\"", &type, &subtype,
                            &params));
  EXPECT_EQ("audio", type);
  EXPECT_EQ("mp2t", subtype);
  ASSERT_EQ(1u, params.size());
  EXPECT_EQ("foo bar", params["codecs"]);

  params.clear();
  ASSERT_TRUE(ParseMimeType("text/vtt; encoding=UTF-8; codecs=stpp", &type,
                            &subtype, &params));
  EXPECT_EQ("text", type);
  EXPECT_EQ("vtt", subtype);
  ASSERT_EQ(2u, params.size());
  EXPECT_EQ("UTF-8", params["encoding"]);
  EXPECT_EQ("stpp", params["codecs"]);


  GoodMimeTest("audio/video ");
  GoodMimeTest("  audio/video");
  GoodMimeTest("audio/video; codecs=");
  GoodMimeTest("audio/video; codecs=\"\"");
  GoodMimeTest("audio/video; codecs=\"foo/bar=r\"");
  GoodMimeTest("audio/video; codecs=\"\"  ; k=v");


  BadMimeTest("");                   // Empty.
  BadMimeTest("video");              // No subtype.
  BadMimeTest("/mp4");               // Empty type.
  BadMimeTest("video?/mp4");         // Type has special chars.
  BadMimeTest("vi deo/mp4");         // Type has special chars.
  BadMimeTest("video/");             // Empty subtype.
  BadMimeTest("video/audio?");       // Subtype has special chars.
  BadMimeTest("video/au dio");       // Subtype has special chars.
  BadMimeTest("video/audio/other");  // Subtype has special chars.

  BadMimeTest("video/audio;");              // No parameter name
  BadMimeTest("video/audio;  ");            // No parameter name
  BadMimeTest("video/audio;key");           // No equals sign.
  BadMimeTest("video/audio;key=value;");    // No parameter name
  BadMimeTest("video/audio;k/y=value");     // Key has special chars.
  BadMimeTest("video/audio;k y=value");     // Key has special chars.
  BadMimeTest("video/audio;key=va/lue");    // Value has special chars.
  BadMimeTest("video/audio;key=va=lue");    // Value has special chars.
  BadMimeTest("video/audio;key=\"");        // No end of quoted string.
  BadMimeTest("video/audio;key=\"\" foo");  // Chars after end of quoted string.
  BadMimeTest("video/audio;key=\"\"foo; k=v");

  ASSERT_TRUE(ParseMimeType("text/vtt; encoding=UTF-8; codecs=stpp", nullptr,
                            nullptr, nullptr));
}


TEST(MediaUtilsTest, IntersectionOfBufferedRanges) {
  EXPECT_EQ(BufferedRanges(),
            IntersectionOfBufferedRanges(std::vector<BufferedRanges>()));

  {
    const BufferedRanges empty_range;
    EXPECT_EQ(empty_range,
              IntersectionOfBufferedRanges(std::vector<BufferedRanges>()));
    EXPECT_EQ(empty_range, IntersectionOfBufferedRanges({empty_range}));
    EXPECT_EQ(empty_range,
              IntersectionOfBufferedRanges({empty_range, empty_range}));
  }

  {
    const BufferedRanges range = {{1, 4}, {7, 10}};
    EXPECT_EQ(range, IntersectionOfBufferedRanges({range}));
  }

  {
    const BufferedRanges range = {{1, 4}, {7, 10}};
    EXPECT_EQ(range, IntersectionOfBufferedRanges({range, range}));
  }

  {
    const BufferedRanges range1 = {{1, 4}, {7, 10}};
    const BufferedRanges range2 = {{1, 4}};
    EXPECT_EQ(range2, IntersectionOfBufferedRanges({range1, range2}));
  }

  {
    const BufferedRanges range1 = {{7, 10}};
    const BufferedRanges range2 = {{1, 4}, {7, 10}};
    EXPECT_EQ(range1, IntersectionOfBufferedRanges({range1, range2}));
  }

  {
    const BufferedRanges range1 = {{1, 4}, {7, 10}};
    const BufferedRanges range2 = {{2, 3}};
    EXPECT_EQ(range2, IntersectionOfBufferedRanges({range1, range2}));
  }

  {
    const BufferedRanges range1 = {{1, 4}};
    const BufferedRanges range2 = {{6, 10}};
    EXPECT_EQ(BufferedRanges(), IntersectionOfBufferedRanges({range1, range2}));
  }

  {
    const BufferedRanges range1 = {{1, 4}, {7, 10}};
    const BufferedRanges range2 = {{3, 6}};
    const BufferedRanges expected = {{3, 4}};
    EXPECT_EQ(expected, IntersectionOfBufferedRanges({range1, range2}));
  }

  {
    const BufferedRanges range1 = {{1, 4}, {7, 10}};
    const BufferedRanges range2 = {{2, 8}};
    const BufferedRanges expected = {{2, 4}, {7, 8}};
    EXPECT_EQ(expected, IntersectionOfBufferedRanges({range1, range2}));
    EXPECT_EQ(expected, IntersectionOfBufferedRanges({range2, range1}));
  }

  {
    const BufferedRanges range1 = {{2, 8}};
    const BufferedRanges range2 = {{0, 6}, {7, 9}};
    const BufferedRanges range3 = {{3, 4}, {5, 6}, {7, 9}};
    const BufferedRanges expected = {{3, 4}, {5, 6}, {7, 8}};
    EXPECT_EQ(expected, IntersectionOfBufferedRanges({range1, range2, range3}));
    EXPECT_EQ(expected, IntersectionOfBufferedRanges({range2, range1, range3}));
    EXPECT_EQ(expected, IntersectionOfBufferedRanges({range3, range1, range3}));
  }
}

TEST(MediaUtilsTest, ConvertMimeToDecodingConfiguration) {
  {
    auto config = ConvertMimeToDecodingConfiguration("video/mp4",
                                                     MediaDecodingType::File);
    EXPECT_EQ(config.type, MediaDecodingType::File);
    EXPECT_EQ(config.audio.content_type, "video/mp4");
    EXPECT_EQ(config.video.content_type, "video/mp4");

    EXPECT_EQ(config.video.width, 0);
    EXPECT_EQ(config.video.height, 0);
    EXPECT_EQ(config.video.framerate, 0);
    EXPECT_EQ(config.audio.channels, 0);
    EXPECT_EQ(config.audio.bitrate, 0);
  }

  {
    const std::string mime =
        "video/mp4; codecs=\"avc1\"; width=200; height=100; "
        "framerate=\"0.0333\"; channels=6; bitrate=2000";
    auto config =
        ConvertMimeToDecodingConfiguration(mime, MediaDecodingType::File);
    EXPECT_EQ(config.type, MediaDecodingType::File);
    EXPECT_EQ(config.audio.content_type, mime);
    EXPECT_EQ(config.video.content_type, mime);

    EXPECT_EQ(config.video.width, 200);
    EXPECT_EQ(config.video.height, 100);
    EXPECT_NEAR(config.video.framerate, 0.0333, 0.0001);
    EXPECT_EQ(config.audio.channels, 6);
    EXPECT_EQ(config.audio.bitrate, 2000);
  }
}

}  // namespace media
}  // namespace shaka
