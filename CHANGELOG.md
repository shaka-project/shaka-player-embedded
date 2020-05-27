## 1.0.0 (2020-05-27)

First full release!  Public API is stable from this point onward.

New Features (not exhaustive):
- Generic media pipeline allowing apps to replace media components
- Offline storage and playback support
- Use native AVPlayer for HLS
  - Controllable with `streaming.useNativeHlsOnSafari` configuration
  - Exposes the AVPlayer instance for app control
- Don't use FFmpeg decoders to reduce binary size
- Removes SDL to reduce binary size
- Better Swift support
- Network request/response filters
- Playback of HEVC content
- Honors sample aspect ratio (SAR)
- Allow setting video fill mode


## 0.1.0-beta (2018-11-30)

Initial beta launch!

New Features (not exhaustive):
- DASH and HLS playback
  - Adaptation support and manual stream switching
  - VOD and Live content
- MP4 and WebM playback
  - With hardware acceleration on iOS with H.264
- Encrypted media playback
  - Clear key built in
  - Plugin system for external DRM providers
