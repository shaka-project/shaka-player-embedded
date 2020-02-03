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

// Include language-agnostic headers.
#include "config_names.h"
#include "macros.h"
#include "shaka_config.h"
#include "version.h"

// Include Objective-C headers if we are compiling Objective-C or Objective-C++.
#if defined(__OBJC__)
#  import "ShakaPlayerStorage.h"
#  import "ShakaPlayer.h"
#  import "ShakaPlayerView2.h"
#  import "error_objc.h"
#  import "manifest_objc.h"
#  import "offline_externs_objc.h"
#  import "player_externs_objc.h"
#  import "stats_objc.h"
#  import "track_objc.h"
#endif

// Include C++ headers if we are compiling in C++ or Objective-C++.
#ifdef __cplusplus
#  include "async_results.h"
#  include "error.h"
#  include "js_manager.h"
#  include "manifest.h"
#  include "offline_externs.h"
#  include "optional.h"
#  include "player.h"
#  include "player_externs.h"
#  ifdef SHAKA_SDL_VIDEO
#    include "sdl_frame_drawer.h"
#  endif
#  include "stats.h"
#  include "storage.h"
#  include "track.h"
#  include "utils.h"
#  include "variant.h"

#  include "eme/configuration.h"
#  include "eme/data.h"
#  include "eme/eme_promise.h"
#  include "eme/implementation.h"
#  include "eme/implementation_factory.h"
#  include "eme/implementation_helper.h"
#  include "eme/implementation_registry.h"

#  include "media/decoder.h"
#  ifdef SHAKA_DEFAULT_MEDIA_PLAYER
#    include "media/default_media_player.h"
#  endif
#  include "media/demuxer.h"
#  include "media/frames.h"
#  include "media/media_capabilities.h"
#  include "media/media_player.h"
#  include "media/proxy_media_player.h"
#  include "media/renderer.h"
#  ifdef SHAKA_SDL_AUDIO
#    include "media/sdl_audio_renderer.h"
#  endif
#  ifdef SHAKA_SDL_VIDEO
#    include "media/sdl_video_renderer.h"
#  endif
#  include "media/stream_info.h"
#  include "media/streams.h"
#  include "media/text_track.h"
#  include "media/vtt_cue.h"
#endif
