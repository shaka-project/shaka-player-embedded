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
#  define SHAKA_MAKE_STRING_(name) @name
#else
#  define SHAKA_MAKE_STRING_(name) name
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
    SHAKA_MAKE_STRING_("drm.retryParameters.maxAttempts")
/**
 * (number) The delay before the first retry for license requests, in
 * milliseconds.
 */
#define kDrmRetryBaseDelay \
    SHAKA_MAKE_STRING_("drm.retryParameters.baseDelay")
/** (number) The multiplier for successive retry delays for license requests. */
#define kDrmRetryBackoffFactor \
    SHAKA_MAKE_STRING_("drm.retryParameters.backoffFactor")
/**
 * (number) The range (as a factor) to fuzz the delay amount for license
 * requests.  For example, 0.5 means 50% above and below.
 */
#define kDrmRetryFuzzFactor \
    SHAKA_MAKE_STRING_("drm.retryParameters.fuzzFactor")
/**
 * (number) The request timeout for license requests, in milliseconds.  Zero
 * means no timeout.
 */
#define kDrmRetryTimeout \
    SHAKA_MAKE_STRING_("drm.retryParameters.timeout")
/** (boolean) Delay sending license requests until playback has started. */
#define kDelayLicenseRequestUntilPlayed \
    SHAKA_MAKE_STRING_("drm.delayLicenseRequestUntilPlayed");

/**
 * (number) The maximum number of requests that will be made for manifest
 * requests.
 */
#define kManifestRetryMaxAttempts \
    SHAKA_MAKE_STRING_("manifest.retryParameters.maxAttempts")
/**
 * (number) The delay before the first retry for manifest requests, in
 * milliseconds.
 */
#define kManifestRetryBaseDelay \
    SHAKA_MAKE_STRING_("manifest.retryParameters.baseDelay")
/**
 * (number) The multiplier for successive retry delays for manifest requests.
 */
#define kManifestRetryBackoffFactor \
    SHAKA_MAKE_STRING_("manifest.retryParameters.backoffFactor")
/**
 * (number) The range (as a factor) to fuzz the delay amount for manifest
 * requests.  For example, 0.5 means 50% above and below.
 */
#define kManifestRetryFuzzFactor \
    SHAKA_MAKE_STRING_("manifest.retryParameters.fuzzFactor")
/**
 * (number) The request timeout for manifest requests, in milliseconds.  Zero
 * means no timeout.
 */
#define kManifestRetryTimeout \
    SHAKA_MAKE_STRING_("manifest.retryParameters.timeout")

/**
 * (string) The URI to fetch for Live clock-synchronization if none is provided
 * in the manifest.
 */
#define kManifestDashClockSyncUri \
    SHAKA_MAKE_STRING_("manifest.dash.clockSyncUri")
/**
 * (boolean) Ignore any DRM info provided in the manifest and only use the info
 * found in the media.  This assumes that all standard key systems are
 * supported.
 */
#define kDashIgnoreDrmInfo SHAKA_MAKE_STRING_("manifest.dash.ignoreDrmInfo")
/**
 * (boolean) If true, xlink failures will result in using the existing content
 * in the tag; if false, xlink failures result in manifest parsing errors.
 */
#define kXlinkFailGracefully \
    SHAKA_MAKE_STRING_("manifest.dash.xlinkFailGracefully")
/**
 * (number) The default presentation delay to use if suggestedPresentationDelay
 * is not found in the manifest.
 */
#define kDashDefaultPresentationDelay \
    SHAKA_MAKE_STRING_("manifest.dash.defaultPresentationDelay")

/**
 * (number) The maximum number of requests that will be made for segment
 * requests.
 */
#define kStreamingRetryMaxAttempts \
    SHAKA_MAKE_STRING_("streaming.retryParameters.maxAttempts")
/**
 * (number) The delay before the first retry for segment requests, in
 * milliseconds.
 */
#define kStreamingRetryBaseDelay \
    SHAKA_MAKE_STRING_("streaming.retryParameters.baseDelay")
/**
 * (number) The multiplier for successive retry delays for segment requests.
 */
#define kStreamingRetryBackoffFactor \
    SHAKA_MAKE_STRING_("streaming.retryParameters.backoffFactor")
/**
 * (number) The range (as a factor) to fuzz the delay amount for segment
 * requests.  For example, 0.5 means 50% above and below.
 */
#define kStreamingRetryFuzzFactor \
    SHAKA_MAKE_STRING_("streaming.retryParameters.fuzzFactor")
/**
 * (number) The request timeout for segment requests, in milliseconds.  Zero
 * means no timeout.
 */
#define kStreamingRetryTimeout \
    SHAKA_MAKE_STRING_("streaming.retryParameters.timeout")

/**
 * (number) The minimum number of seconds of content that must buffer before we
 * can begin playback or can continue playback after we enter into a
 * buffering state.
 */
#define kRebufferingGoal SHAKA_MAKE_STRING_("streaming.rebufferingGoal")
/**
 * (number) The number of seconds of content that we will attempt to buffer
 * ahead of the playhead.
 */
#define kBufferingGoal SHAKA_MAKE_STRING_("streaming.bufferingGoal")
/**
 * (number) The maximum number of seconds of content that we will keep in buffer
 * behind the playhead.
 */
#define kBufferBehind SHAKA_MAKE_STRING_("streaming.bufferBehind")
/**
 * (boolean) If true, the player will ignore text stream failures and continue
 * playing other streams.
 */
#define kIgnoreTextStreamFailures \
    SHAKA_MAKE_STRING_("streaming.ignoreTextStreamFailures")
/**
 * (boolean) If true, always stream text tracks, regardless of whether or not
 * they are shown.
 */
#define kAlwaysStreamText SHAKA_MAKE_STRING_("streaming.alwaysStreamText")
/**
 * (boolean) If true, adjust the start time backwards so it is at the start of a
 * segment.
 */
#define kStartAtSegmentBoundary \
    SHAKA_MAKE_STRING_("streaming.startAtSegmentBoundary")
/**
 * (number) The limit (in seconds) for a gap in the media to be considered
 * "small". Small gaps are jumped automatically without events.
 */
#define kSmallGapLimit SHAKA_MAKE_STRING_("streaming.smallGapLimit")
/** (boolean) If true, jump large gaps in addition to small gaps. */
#define kJumpLargeGaps SHAKA_MAKE_STRING_("streaming.jumpLargeGaps")
/**
 * (number) The number of seconds to adjust backward when seeking to exactly
 * the duration.
 */
#define kDurationBackoff SHAKA_MAKE_STRING_("streaming.durationBackoff")

/** (boolean) Whether ABR is currently enabled. */
#define kAbrEnabled SHAKA_MAKE_STRING_("abr.enabled")
/** (number) The default estimate of the network bandwidth, in bit/sec. */
#define kDefaultBandwidthEstimate \
    SHAKA_MAKE_STRING_("abr.defaultBandwidthEstimate")
/** (number) The minimum number of seconds between ABR switches. */
#define kAbrSwitchInterval SHAKA_MAKE_STRING_("abr.switchInterval")
/**
 * (number) The fraction of the estimated bandwidth which we should try to use
 * when upgrading.
 */
#define kAbrUpgradeTarget SHAKA_MAKE_STRING_("abr.bandwidthUpgradeTarget")
/**
 * (number) The largest fraction of the estimated bandwidth we should use. We
 * should downgrade to avoid this.
 */
#define kAbrDowngradeTarget SHAKA_MAKE_STRING_("abr.bandwidthDowngradeTarget")

/**
 * (number) The minimum width of a video track that will be chosen by ABR; if
 * none match the restrictions, ABR will choose a random track.
 */
#define kAbrMinWidth SHAKA_MAKE_STRING_("abr.restrictions.minWidth")
/**
 * (number) The maximum width of a video track that will be chosen by ABR; if
 * none match the restrictions, ABR will choose a random track.
 */
#define kAbrMaxWidth SHAKA_MAKE_STRING_("abr.restrictions.maxWidth")
/**
 * (number) The minimum height of a video track that will be chosen by ABR; if
 * none match the restrictions, ABR will choose a random track.
 */
#define kAbrMinHeight SHAKA_MAKE_STRING_("abr.restrictions.minHeight")
/**
 * (number) The maximum height of a video track that will be chosen by ABR; if
 * none match the restrictions, ABR will choose a random track.
 */
#define kAbrMaxHeight SHAKA_MAKE_STRING_("abr.restrictions.maxHeight")
/**
 * (number) The minimum number of total pixels of a video track that will be
 * chosen by ABR; if none match the restrictions, ABR will choose a random
 * track.
 */
#define kAbrMinPixels SHAKA_MAKE_STRING_("abr.restrictions.minPixels")
/**
 * (number) The maximum number of total pixels of a video track that will be
 * chosen by ABR; if none match the restrictions, ABR will choose a random
 * track.
 */
#define kAbrMaxPixels SHAKA_MAKE_STRING_("abr.restrictions.maxPixels")
/**
 * (number) The minimum total bandwidth in bit/sec that will be chosen by ABR;
 * if none match the restrictions, ABR will choose a random track.
 */
#define kAbrMinBandwidth SHAKA_MAKE_STRING_("abr.restrictions.minBandwidth")
/**
 * (number) The maximum total bandwidth in bit/sec that will be chosen by ABR;
 * if none match the restrictions, ABR will choose a random track.
 */
#define kAbrMaxBandwidth SHAKA_MAKE_STRING_("abr.restrictions.maxBandwidth")

/** (string) The preferred language to use for audio tracks. */
#define kPreferredAudioLanguage SHAKA_MAKE_STRING_("preferredAudioLanguage")
/** (string) The preferred language to use for text tracks. */
#define kPreferredTextLanguage SHAKA_MAKE_STRING_("preferredTextLanguage")
/** (string) The preferred role to use for variant tracks. */
#define kPreferredVariantRole SHAKA_MAKE_STRING_("preferredVariantRole")
/** (string) The preferred role to use for text tracks. */
#define kPreferredTextRole SHAKA_MAKE_STRING_("preferredTextRole")
/** (number) The preferred number of audio channels. */
#define kPreferredAudioChannelCount \
    SHAKA_MAKE_STRING_("preferredAudioChannelCount")

/** (number) The minimum width of a video track that can be played. */
#define kMinWidth SHAKA_MAKE_STRING_("restrictions.minWidth")
/** (number) The maximum width of a video track that can be played. */
#define kMaxWidth SHAKA_MAKE_STRING_("restrictions.maxWidth")
/** (number) The minimum height of a video track that can be played. */
#define kMinHeight SHAKA_MAKE_STRING_("restrictions.minHeight")
/** (number) The maximum height of a video track that can be played. */
#define kMaxHeight SHAKA_MAKE_STRING_("restrictions.maxHeight")
/**
 * (number) The minimum number of total pixels of a video track that can be
 * played.
 */
#define kMinPixels SHAKA_MAKE_STRING_("restrictions.minPixels")
/**
 * (number) The maximum number of total pixels of a video track that can be
 * played.
 */
#define kMaxPixels SHAKA_MAKE_STRING_("restrictions.maxPixels")
/** (number) The minimum total bandwidth in bit/sec that can be played. */
#define kMinBandwidth SHAKA_MAKE_STRING_("restrictions.minBandwidth")
/** (number) The maximum total bandwidth in bit/sec that can be played. */
#define kMaxBandwidth SHAKA_MAKE_STRING_("restrictions.maxBandwidth")

/** (number) The start of restricted seek range, in seconds. */
#define kPlayRangeStart SHAKA_MAKE_STRING_("playRangeStart")
/** (number) The end of restricted seek range, in seconds. */
#define kPlayRangeEnd SHAKA_MAKE_STRING_("playRangeEnd")

/** @} */

#ifndef __OBJC__
}  // namespace shaka
#endif

#endif  // SHAKA_EMBEDDED_CONFIG_NAMES_H_
