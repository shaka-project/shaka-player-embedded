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

ShakaRect MakeRect(int x, int y, int w, int h) {
  return ShakaRect{x, y, w, h};
}

}  // namespace

TEST(ShakaUtilsTest, FitVideoToRegion_Original) {
  ShakaRect src;
  ShakaRect dest;
  auto run = [&](ShakaRect frame, ShakaRect bounds) {
    FitVideoToRegion(frame, bounds, media::VideoFillMode::Original, &src,
                     &dest);
  };

  // Original smaller than window.
  run({0, 0, 4, 4}, {0, 0, 10, 10});
  EXPECT_EQ(src, MakeRect(0, 0, 4, 4));
  EXPECT_EQ(dest, MakeRect(3, 3, 4, 4));
  run({5, 5, 4, 4}, {5, 5, 10, 10});
  EXPECT_EQ(src, MakeRect(5, 5, 4, 4));
  EXPECT_EQ(dest, MakeRect(5 + 3, 5 + 3, 4, 4));
  run({1, 2, 3, 4}, {10, 11, 12, 13});
  EXPECT_EQ(src, MakeRect(1, 2, 3, 4));
  EXPECT_EQ(dest, MakeRect(10 + 4, 11 + 4, 3, 4));
  // Original larger than window.
  run({0, 0, 10, 10}, {0, 0, 4, 4});
  EXPECT_EQ(src, MakeRect(3, 3, 4, 4));
  EXPECT_EQ(dest, MakeRect(0, 0, 4, 4));
  run({5, 5, 10, 10}, {5, 5, 4, 4});
  EXPECT_EQ(src, MakeRect(8, 8, 4, 4));
  EXPECT_EQ(dest, MakeRect(5, 5, 4, 4));

  // Same size.
  run({2, 2, 5, 5}, {2, 2, 5, 5});
  EXPECT_EQ(src, MakeRect(2, 2, 5, 5));
  EXPECT_EQ(dest, MakeRect(2, 2, 5, 5));

  // Same size but different offsets.
  run({1, 5, 5, 5}, {4, 7, 5, 5});
  EXPECT_EQ(src, MakeRect(1, 5, 5, 5));
  EXPECT_EQ(dest, MakeRect(4, 7, 5, 5));
}

TEST(ShakaUtilsTest, FitVideoToRegion_Stretch) {
  ShakaRect src;
  ShakaRect dest;
  auto run = [&](ShakaRect frame, ShakaRect bounds) {
    FitVideoToRegion(frame, bounds, media::VideoFillMode::Stretch, &src, &dest);
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
  ShakaRect src;
  ShakaRect dest;
  auto run = [&](ShakaRect frame, ShakaRect bounds) {
    FitVideoToRegion(frame, bounds, media::VideoFillMode::Zoom, &src, &dest);
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
  ShakaRect src;
  ShakaRect dest;
  auto run = [&](ShakaRect frame, ShakaRect bounds) {
    FitVideoToRegion(frame, bounds, media::VideoFillMode::MaintainRatio, &src,
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

}  // namespace shaka
