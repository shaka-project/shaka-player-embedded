// Copyright 2019 Google LLC
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

#include "shaka/utils.h"

namespace shaka {

namespace {

ShakaRect<uint32_t> MakeRect(int x, int y, int w, int h) {
  return ShakaRect<uint32_t>{x, y, w, h};
}

}  // namespace

TEST(ShakaUtilsTest, FitVideoToRegion_Stretch) {
  ShakaRect<uint32_t> src;
  ShakaRect<uint32_t> dest;
  auto run = [&](ShakaRect<uint32_t> frame, ShakaRect<uint32_t> bounds) {
    FitVideoToRegion(frame, bounds, {0, 0}, VideoFillMode::Stretch, &src,
                     &dest);
    // Stretch should always use the whole input and fill the whole output.
    EXPECT_EQ(src, frame);
    EXPECT_EQ(dest, bounds);
  };

  // Video smaller, aspect ratio same.
  run({0, 0, 4, 4}, {0, 0, 8, 8});
  run({3, 8, 4, 3}, {5, 1, 8, 6});
  // Video bigger, aspect ratio same.
  run({3, 8, 8, 10}, {5, 1, 4, 5});

  run({3, 8, 4, 10}, {5, 1, 4, 4});
  run({3, 8, 4, 10}, {5, 1, 2, 10});

  // Same size.
  run({2, 2, 5, 5}, {2, 2, 5, 5});
  // Same size but different offsets.
  run({1, 4, 5, 5}, {8, 9, 5, 5});
}

TEST(ShakaUtilsTest, FitVideoToRegion_Zoom) {
  ShakaRect<uint32_t> src;
  ShakaRect<uint32_t> dest;
  auto run = [&](ShakaRect<uint32_t> frame, ShakaRect<uint32_t> bounds) {
    FitVideoToRegion(frame, bounds, {0, 0}, VideoFillMode::Zoom, &src, &dest);
    // Zoom should always fill the whole output.
    EXPECT_EQ(dest, bounds);
  };

  // Video smaller, aspect ratio same.
  run({0, 0, 4, 4}, {0, 0, 8, 8});
  EXPECT_EQ(src, MakeRect(0, 0, 4, 4));
  run({3, 8, 4, 3}, {5, 1, 8, 6});
  EXPECT_EQ(src, MakeRect(3, 8, 4, 3));
  // Video bigger, aspect ratio same.
  run({3, 8, 8, 10}, {5, 1, 4, 5});
  EXPECT_EQ(src, MakeRect(3, 8, 8, 10));

  // Fit to width, clip height
  run({3, 8, 4, 10}, {5, 1, 4, 4});
  EXPECT_EQ(src, MakeRect(3, 8 + 3, 4, 4));
  run({3, 8, 4, 10}, {5, 1, 8, 8});
  EXPECT_EQ(src, MakeRect(3, 8 + 3, 4, 4));

  // Fit to height, clip width
  run({3, 8, 10, 4}, {5, 1, 4, 4});
  EXPECT_EQ(src, MakeRect(3 + 3, 8, 4, 4));
  run({3, 8, 10, 4}, {5, 1, 8, 8});
  EXPECT_EQ(src, MakeRect(3 + 3, 8, 4, 4));

  // Same size.
  run({2, 2, 5, 5}, {2, 2, 5, 5});
  EXPECT_EQ(src, MakeRect(2, 2, 5, 5));
  // Same size but different offsets.
  run({1, 4, 5, 5}, {8, 9, 5, 5});
  EXPECT_EQ(src, MakeRect(1, 4, 5, 5));
}

TEST(ShakaUtilsTest, FitVideoToRegion_MaintainRatio) {
  ShakaRect<uint32_t> src;
  ShakaRect<uint32_t> dest;
  auto run = [&](ShakaRect<uint32_t> frame, ShakaRect<uint32_t> bounds) {
    FitVideoToRegion(frame, bounds, {0, 0}, VideoFillMode::MaintainRatio, &src,
                     &dest);
    // MaintainRatio should always use the whole source.
    EXPECT_EQ(src, frame);
  };

  // Video smaller, aspect ratio same.
  run({0, 0, 4, 4}, {0, 0, 8, 8});
  EXPECT_EQ(dest, MakeRect(0, 0, 8, 8));
  run({3, 8, 4, 3}, {5, 1, 8, 6});
  EXPECT_EQ(dest, MakeRect(5, 1, 8, 6));
  // Video bigger, aspect ratio same.
  run({3, 8, 8, 10}, {5, 1, 4, 5});
  EXPECT_EQ(dest, MakeRect(5, 1, 4, 5));

  // Fit to width, black bars around top
  run({3, 8, 4, 4}, {5, 1, 4, 10});
  EXPECT_EQ(dest, MakeRect(5, 1 + 3, 4, 4));
  run({3, 8, 4, 4}, {5, 1, 8, 20});
  EXPECT_EQ(dest, MakeRect(5, 1 + 6, 8, 8));

  // Fit to height, black bars around sides
  run({3, 8, 4, 4}, {5, 1, 10, 4});
  EXPECT_EQ(dest, MakeRect(5 + 3, 1, 4, 4));
  run({3, 8, 4, 4}, {5, 1, 20, 8});
  EXPECT_EQ(dest, MakeRect(5 + 6, 1, 8, 8));

  // Same size.
  run({2, 2, 5, 5}, {2, 2, 5, 5});
  EXPECT_EQ(dest, MakeRect(2, 2, 5, 5));
  // Same size but different offsets.
  run({1, 4, 5, 5}, {8, 9, 5, 5});
  EXPECT_EQ(dest, MakeRect(8, 9, 5, 5));
}

TEST(ShakaUtilsTest, FitVideoToRegion_SampleAspectRatio) {
  ShakaRect<uint32_t> src;
  ShakaRect<uint32_t> dest;

  FitVideoToRegion({0, 0, 4, 4}, {0, 0, 8, 8}, {2, 1},
                   VideoFillMode::MaintainRatio, &src, &dest);
  EXPECT_EQ(src, MakeRect(0, 0, 4, 4));
  EXPECT_EQ(dest, MakeRect(0, 2, 8, 4));
  FitVideoToRegion({0, 0, 4, 4}, {0, 0, 8, 8}, {1, 2},
                   VideoFillMode::MaintainRatio, &src, &dest);
  EXPECT_EQ(src, MakeRect(0, 0, 4, 4));
  EXPECT_EQ(dest, MakeRect(2, 0, 4, 8));

  FitVideoToRegion({0, 0, 4, 4}, {0, 0, 8, 8}, {2, 1}, VideoFillMode::Zoom,
                   &src, &dest);
  EXPECT_EQ(src, MakeRect(1, 0, 2, 4));
  EXPECT_EQ(dest, MakeRect(0, 0, 8, 8));
  FitVideoToRegion({0, 0, 4, 4}, {0, 0, 8, 8}, {1, 2}, VideoFillMode::Zoom,
                   &src, &dest);
  EXPECT_EQ(src, MakeRect(0, 1, 4, 2));
  EXPECT_EQ(dest, MakeRect(0, 0, 8, 8));
}

TEST(ShakaUtilsTest, Rational) {
  Rational<int> one(1, 1);
  Rational<int> two(2, 1);
  Rational<int> half(1, 2);
  Rational<int> third(1, 3);
  Rational<int> sixth(1, 6);

  EXPECT_EQ(one, one);
  EXPECT_EQ(half * 2, one);
  EXPECT_EQ(2 * half, one);
  EXPECT_EQ(two * half, one);
  EXPECT_EQ(half * third, sixth);
  EXPECT_EQ(one / two, half);
  EXPECT_EQ(one / 2, half);
  EXPECT_EQ(1 / two, half);
  EXPECT_NE(one, two);

  EXPECT_EQ(static_cast<double>(half), 0.5);
  EXPECT_EQ(two.truncate(), 2);
  EXPECT_EQ(two.inverse(), half);
}

}  // namespace shaka
