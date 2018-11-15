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

#ifndef SHAKA_EMBEDDED_CONFIG_NAMES_H_
#define SHAKA_EMBEDDED_CONFIG_NAMES_H_

#ifdef __OBJC__
#  include <CoreFoundation/CoreFoundation.h>
#else
namespace shaka {
#endif

#ifdef __OBJC__
#  define _SHAKA_MAKE_STRING(name) @name
#else
#  define _SHAKA_MAKE_STRING(name) name
#endif

/**
 * @defgroup config Configuration Keys
 * @ingroup exported
 * Constant names for configuration key values.
 *
 * These variables contain the string constants for the configurations that the
 * Player supports.
 * @{
 */

/**
 * (number) The maximum number of requests that will be made for license
 * requests.
 */
#define kDrmRetryMaxAttempts \
    _SHAKA_MAKE_STRING("drm.retryParameters.maxAttempts")
/**
 * (number) The delay before the first retry for license requests, in
 * milliseconds.
 */
#define kDrmRetryBaseDelay \
    _SHAKA_MAKE_STRING("drm.retryParameters.baseDelay")
/** (number) The multiplier for successive retry delays for license requests. */
#define kDrmRetryBackoffFactor \
    _SHAKA_MAKE_STRING("drm.retryParameters.backoffFactor")
/**
 * (number) The range (as a factor) to fuzz the delay amount for license
 * requests.  For example, 0.5 means 50% above and below.
 */
#define kDrmRetryFuzzFactor \
    _SHAKA_MAKE_STRING("drm.retryParameters.fuzzFactor")
/**
 * (number) The request timeout for license requests, in milliseconds.  Zero
 * means no timeout.
 */
#define kDrmRetryTimeout \
    _SHAKA_MAKE_STRING("drm.retryParameters.timeout")
/** (boolean) Delay sending license requests until playback has started. */
#define kDelayLicenseRequestUntilPlayed \
    _SHAKA_MAKE_STRING("drm.delayLicenseRequestUntilPlayed");

/**
 * (number) The maximum number of requests that will be made for manifest
 * requests.
 */
#define kManifestRetryMaxAttempts \
    _SHAKA_MAKE_STRING("manifest.retryParameters.maxAttempts")
/**
 * (number) The delay before the first retry for manifest requests, in
 * milliseconds.
 */
#define kManifestRetryBaseDelay \
    _SHAKA_MAKE_STRING("manifest.retryParameters.baseDelay")
/**
 * (number) The multiplier for successive retry delays for manifest requests.
 */
#define kManifestRetryBackoffFactor \
    _SHAKA_MAKE_STRING("manifest.retryParameters.backoffFactor")
/**
 * (number) The range (as a factor) to fuzz the delay amount for manifest
 * requests.  For example, 0.5 means 50% above and below.
 */
#define kManifestRetryFuzzFactor \
    _SHAKA_MAKE_STRING("manifest.retryParameters.fuzzFactor")
/**
 * (number) The request timeout for manifest requests, in milliseconds.  Zero
 * means no timeout.
 */
#define kManifestRetryTimeout \
    _SHAKA_MAKE_STRING("manifest.retryParameters.timeout")

/**
 * (string) The URI to fetch for Live clock-synchronization if none is provided
 * in the manifest.
 */
#define kManifestDashClockSyncUri \
    _SHAKA_MAKE_STRING("manifest.dash.clockSyncUri")
/**
 * (boolean) Ignore any DRM info provided in the manifest and only use the info
 * found in the media.  This assumes that all standard key systems are
 * supported.
 */
#define kDashIgnoreDrmInfo _SHAKA_MAKE_STRING("manifest.dash.ignoreDrmInfo")
/**
 * (boolean) If true, xlink failures will result in using the existing content
 * in the tag; if false, xlink failures result in manifest parsing errors.
 */
#define kXlinkFailGracefully \
    _SHAKA_MAKE_STRING("manifest.dash.xlinkFailGracefully")
/**
 * (number) The default presentation delay to use if suggestedPresentationDelay
 * is not found in the manifest.
 */
#define kDashDefaultPresentationDelay \
    _SHAKA_MAKE_STRING("manifest.dash.defaultPresentationDelay")

/**
 * (number) The maximum number of requests that will be made for segment
 * requests.
 */
#define kStreamingRetryMaxAttempts \
    _SHAKA_MAKE_STRING("streaming.retryParameters.maxAttempts")
/**
 * (number) The delay before the first retry for segment requests, in
 * milliseconds.
 */
#define kStreamingRetryBaseDelay \
    _SHAKA_MAKE_STRING("streaming.retryParameters.baseDelay")
/**
 * (number) The multiplier for successive retry delays for segment requests.
 */
#define kStreamingRetryBackoffFactor \
    _SHAKA_MAKE_STRING("streaming.retryParameters.backoffFactor")
/**
 * (number) The range (as a factor) to fuzz the delay amount for segment
 * requests.  For example, 0.5 means 50% above and below.
 */
#define kStreamingRetryFuzzFactor \
    _SHAKA_MAKE_STRING("streaming.retryParameters.fuzzFactor")
/**
 * (number) The request timeout for segment requests, in milliseconds.  Zero
 * means no timeout.
 */
#define kStreamingRetryTimeout \
    _SHAKA_MAKE_STRING("streaming.retryParameters.timeout")

/**
 * (number) The minimum number of seconds of content that must buffer before we
 * can begin playback or can continue playback after we enter into a
 * buffering state.
 */
#define kRebufferingGoal _SHAKA_MAKE_STRING("streaming.rebufferingGoal")
/**
 * (number) The number of seconds of content that we will attempt to buffer
 * ahead of the playhead.
 */
#define kBufferingGoal _SHAKA_MAKE_STRING("streaming.bufferingGoal")
/**
 * (number) The maximum number of seconds of content that we will keep in buffer
 * behind the playhead.
 */
#define kBufferBehind _SHAKA_MAKE_STRING("streaming.bufferBehind")
/**
 * (boolean) If true, the player will ignore text stream failures and continue
 * playing other streams.
 */
#define kIgnoreTextStreamFailures \
    _SHAKA_MAKE_STRING("streaming.ignoreTextStreamFailures")
/**
 * (boolean) If true, always stream text tracks, regardless of whether or not
 * they are shown.
 */
#define kAlwaysStreamText _SHAKA_MAKE_STRING("streaming.alwaysStreamText")
/**
 * (boolean) If true, adjust the start time backwards so it is at the start of a
 * segment.
 */
#define kStartAtSegmentBoundary \
    _SHAKA_MAKE_STRING("streaming.startAtSegmentBoundary")
/**
 * (number) The limit (in seconds) for a gap in the media to be considered
 * "small". Small gaps are jumped automatically without events.
 */
#define kSmallGapLimit _SHAKA_MAKE_STRING("streaming.smallGapLimit")
/** (boolean) If true, jump large gaps in addition to small gaps. */
#define kJumpLargeGaps _SHAKA_MAKE_STRING("streaming.jumpLargeGaps")
/**
 * (number) The number of seconds to adjust backward when seeking to exactly
 * the duration.
 */
#define kDurationBackoff _SHAKA_MAKE_STRING("streaming.durationBackoff")

/** (boolean) Whether ABR is currently enabled. */
#define kAbrEnabled _SHAKA_MAKE_STRING("abr.enabled")
/** (number) The default estimate of the network bandwidth, in bit/sec. */
#define kDefaultBandwidthEstimate \
    _SHAKA_MAKE_STRING("abr.defaultBandwidthEstimate")
/** (number) The minimum number of seconds between ABR switches. */
#define kAbrSwitchInterval _SHAKA_MAKE_STRING("abr.switchInterval")
/**
 * (number) The fraction of the estimated bandwidth which we should try to use
 * when upgrading.
 */
#define kAbrUpgradeTarget _SHAKA_MAKE_STRING("abr.bandwidthUpgradeTarget")
/**
 * (number) The largest fraction of the estimated bandwidth we should use. We
 * should downgrade to avoid this.
 */
#define kAbrDowngradeTarget _SHAKA_MAKE_STRING("abr.bandwidthDowngradeTarget")

/**
 * (number) The minimum width of a video track that will be chosen by ABR; if
 * none match the restrictions, ABR will choose a random track.
 */
#define kAbrMinWidth _SHAKA_MAKE_STRING("abr.restrictions.minWidth")
/**
 * (number) The maximum width of a video track that will be chosen by ABR; if
 * none match the restrictions, ABR will choose a random track.
 */
#define kAbrMaxWidth _SHAKA_MAKE_STRING("abr.restrictions.maxWidth")
/**
 * (number) The minimum height of a video track that will be chosen by ABR; if
 * none match the restrictions, ABR will choose a random track.
 */
#define kAbrMinHeight _SHAKA_MAKE_STRING("abr.restrictions.minHeight")
/**
 * (number) The maximum height of a video track that will be chosen by ABR; if
 * none match the restrictions, ABR will choose a random track.
 */
#define kAbrMaxHeight _SHAKA_MAKE_STRING("abr.restrictions.maxHeight")
/**
 * (number) The minimum number of total pixels of a video track that will be
 * chosen by ABR; if none match the restrictions, ABR will choose a random
 * track.
 */
#define kAbrMinPixels _SHAKA_MAKE_STRING("abr.restrictions.minPixels")
/**
 * (number) The maximum number of total pixels of a video track that will be
 * chosen by ABR; if none match the restrictions, ABR will choose a random
 * track.
 */
#define kAbrMaxPixels _SHAKA_MAKE_STRING("abr.restrictions.maxPixels")
/**
 * (number) The minimum total bandwidth in bit/sec that will be chosen by ABR;
 * if none match the restrictions, ABR will choose a random track.
 */
#define kAbrMinBandwidth _SHAKA_MAKE_STRING("abr.restrictions.minBandwidth")
/**
 * (number) The maximum total bandwidth in bit/sec that will be chosen by ABR;
 * if none match the restrictions, ABR will choose a random track.
 */
#define kAbrMaxBandwidth _SHAKA_MAKE_STRING("abr.restrictions.maxBandwidth")

/** (string) The preferred language to use for audio tracks. */
#define kPreferredAudioLanguage _SHAKA_MAKE_STRING("preferredAudioLanguage")
/** (string) The preferred language to use for text tracks. */
#define kPreferredTextLanguage _SHAKA_MAKE_STRING("preferredTextLanguage")
/** (string) The preferred role to use for variant tracks. */
#define kPreferredVariantRole _SHAKA_MAKE_STRING("preferredVariantRole")
/** (string) The preferred role to use for text tracks. */
#define kPreferredTextRole _SHAKA_MAKE_STRING("preferredTextRole")
/** (number) The preferred number of audio channels. */
#define kPreferredAudioChannelCount \
    _SHAKA_MAKE_STRING("preferredAudioChannelCount")

/** (number) The minimum width of a video track that can be played. */
#define kMinWidth _SHAKA_MAKE_STRING("restrictions.minWidth")
/** (number) The maximum width of a video track that can be played. */
#define kMaxWidth _SHAKA_MAKE_STRING("restrictions.maxWidth")
/** (number) The minimum height of a video track that can be played. */
#define kMinHeight _SHAKA_MAKE_STRING("restrictions.minHeight")
/** (number) The maximum height of a video track that can be played. */
#define kMaxHeight _SHAKA_MAKE_STRING("restrictions.maxHeight")
/**
 * (number) The minimum number of total pixels of a video track that can be
 * played.
 */
#define kMinPixels _SHAKA_MAKE_STRING("restrictions.minPixels")
/**
 * (number) The maximum number of total pixels of a video track that can be
 * played.
 */
#define kMaxPixels _SHAKA_MAKE_STRING("restrictions.maxPixels")
/** (number) The minimum total bandwidth in bit/sec that can be played. */
#define kMinBandwidth _SHAKA_MAKE_STRING("restrictions.minBandwidth")
/** (number) The maximum total bandwidth in bit/sec that can be played. */
#define kMaxBandwidth _SHAKA_MAKE_STRING("restrictions.maxBandwidth")

/** (number) The start of restricted seek range, in seconds. */
#define kPlayRangeStart _SHAKA_MAKE_STRING("playRangeStart")
/** (number) The end of restricted seek range, in seconds. */
#define kPlayRangeEnd _SHAKA_MAKE_STRING("playRangeEnd")

/** @} */

#ifndef __OBJC__
}  // namespace shaka
#endif

#endif  // SHAKA_EMBEDDED_CONFIG_NAMES_H_
