// Copyright 2016 Google LLC
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

#ifndef OS_IOS
#  include <SDL2/SDL.h>
#endif
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <stdlib.h>
#ifdef USING_V8
#  include <v8.h>
#endif

#include <algorithm>
#include <fstream>
#include <functional>
#include <string>

#include "shaka/js_manager.h"
#include "shaka/media/default_media_player.h"
#include "shaka/media/media_player.h"
#include "shaka/media/renderer.h"
#include "src/core/js_manager_impl.h"
#include "src/mapping/callback.h"
#include "src/mapping/register_member.h"
#include "src/test/global_fields.h"
#include "src/test/js_test_fixture.h"
#include "src/test/media_files.h"
#include "src/util/file_system.h"

namespace shaka {

// Defined in generated code by //shaka/test/embed_tests.py
void LoadJsTests();

media::DefaultMediaPlayer* g_media_player = nullptr;
JsManager* g_js_manager = nullptr;

namespace {

DEFINE_bool(no_colors, false, "Don't print colors in test output.");
#ifdef USING_V8
DEFINE_string(v8_flags, "", "Pass the given flags to V8.");
#endif

class DummyRenderer : public media::VideoRenderer, public media::AudioRenderer {
 public:
  void SetPlayer(const media::MediaPlayer* player) override {}
  void Attach(const media::DecodedStream* stream) override {}
  void Detach() override {}

  double Volume() const override {
    return 0;
  }
  void SetVolume(double volume) override {}
  bool Muted() const override {
    return false;
  }
  void SetMuted(bool muted) override {}

  media::VideoPlaybackQuality VideoPlaybackQuality() const override {
    return media::VideoPlaybackQuality();
  }
  bool SetVideoFillMode(VideoFillMode mode) override {
    return false;
  }
};

int RunTests(int argc, char** argv) {
#ifdef OS_IOS
  const std::string dynamic_data_dir = std::string(getenv("HOME")) + "/Library";
  const std::string static_data_dir = ".";
#else
  const std::string dynamic_data_dir = util::FileSystem::DirName(argv[0]);
  const std::string static_data_dir = dynamic_data_dir;
#endif

  // Setup a dummy MediaPlayer instance that is used for support checking.
  DummyRenderer renderer;
  media::DefaultMediaPlayer player(&renderer, &renderer);
  media::MediaPlayer::SetMediaPlayerForSupportChecks(&player);
  g_media_player = &player;

  // Init gflags.
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  // Init logging.
  FLAGS_alsologtostderr = true;
  google::InitGoogleLogging(argv[0]);

#ifdef USING_V8
  // Expose GC to tests (must be done before initializing V8).
  const std::string flags_for_v8 = "--expose-gc";
  v8::V8::SetFlagsFromString(flags_for_v8.c_str(), flags_for_v8.length());
  v8::V8::SetFlagsFromString(FLAGS_v8_flags.c_str(), FLAGS_v8_flags.length());
#endif

  // Init gtest.
  if (FLAGS_no_colors) {
    testing::FLAGS_gtest_color = "no";
    setenv("AV_LOG_FORCE_NOCOLOR", "1", 0);
  }
  if (testing::FLAGS_gtest_output.empty()) {
    const std::string file = util::FileSystem::GetPathForDynamicFile(
        dynamic_data_dir, "TESTS-results.xml");
    testing::FLAGS_gtest_output = "xml:" + file;
  }
  testing::InitGoogleTest(&argc, argv);

  // Find the location of the media files.
  InitMediaFiles(argv[0]);


  // Start the main JavaScript engine that contains the JavaScript tests.
  JsManager::StartupOptions opts;
  opts.dynamic_data_dir = dynamic_data_dir;
  opts.static_data_dir = static_data_dir;
  opts.is_static_relative_to_bundle = true;
  JsManager engine(opts);
  g_js_manager = &engine;

  JsManagerImpl::Instance()->MainThread()->AddInternalTask(
      TaskPriority::Immediate, "", PlainCallbackTask(&RegisterTestFixture));

  LoadJsTests();

  // Run all the tests.
  return RUN_ALL_TESTS();
}

}  // namespace

}  // namespace shaka

int main(int argc, char** argv) {
  const int code = shaka::RunTests(argc, argv);
  if (code == 0)
    fprintf(stderr, "TEST RESULTS: PASS\n");
  else
    fprintf(stderr, "TEST RESULTS: FAIL\n");
  return code;
}
