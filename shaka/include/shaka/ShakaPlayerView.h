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

#ifndef SHAKA_EMBEDDED_SHAKA_PLAYER_VIEW_H_
#define SHAKA_EMBEDDED_SHAKA_PLAYER_VIEW_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "macros.h"
#include "manifest_objc.h"
#include "player_externs_objc.h"
#include "stats_objc.h"
#include "track_objc.h"

typedef void (^ShakaPlayerErrorCallback)(NSString *errorMessage, NSString *errorContext);
typedef void (^ShakaPlayerBufferingCallback)(BOOL buffering);
typedef void (^ShakaPlayerAsyncBlock)(void);

typedef NS_ENUM(NSInteger, ShakaPlayerLogLevel) {
  // These have the same values as shaka.log.Level.
  ShakaPlayerLogLevelNone = 0,
  ShakaPlayerLogLevelError = 1,
  ShakaPlayerLogLevelWarning = 2,
  ShakaPlayerLogLevelInfo = 3,
  ShakaPlayerLogLevelDebug = 4,
  ShakaPlayerLogLevelV1 = 5,
  ShakaPlayerLogLevelV2 = 6
};

/**
 * A view that encapsulates an instance of Shaka Player, handles rendering,
 * and exposes controls.
 * @ingroup player
 */
SHAKA_EXPORT
@interface ShakaPlayerView : UIView

/** Plays the video. */
- (void)play;

/** Pauses the video. */
- (void)pause;

/** Whether the video is currently paused. */
@property(atomic, readonly) BOOL paused;

/** Whether the video is currently ended. */
@property(atomic, readonly) BOOL ended;

/** Whether the video is currently seeking. */
@property(atomic, readonly) BOOL seeking;

/** The duration of the video, or 0 if nothing is loaded. */
@property(atomic, readonly) double duration;

/** The current playback rate of the video, or 1 if nothing is loaded. */
@property(atomic) double playbackRate;

/** The current time of the video, or 0 if nothing is loaded. */
@property(atomic) double currentTime;

/** The current volume of the video, or 0 if nothing is loaded. */
@property(atomic) double volume;

/** Whether the audio is currently muted. */
@property(atomic) BOOL muted;

/**
 * A callback that is called when the player errors.
 * Note that this might be called on a background thread.
 */
@property(atomic) ShakaPlayerErrorCallback errorCallback;

/**
 * A callback that is called when the player's buffering state changes.
 * This will always be called on the main queue.
 */
@property(atomic) ShakaPlayerBufferingCallback bufferingCallback;


/**
 * The log level of the JavaScript Shaka Player.  Logging only works if the
 * Shaka Player JS file is in a debug build.
 */
@property(atomic) ShakaPlayerLogLevel logLevel;

/** The version of Shaka Player being used, as a string. */
@property(atomic, readonly) NSString *playerVersion;

/** Whether the video is currently audio-only. */
@property(atomic, readonly) BOOL isAudioOnly;

/** Whether the video is a livestream. */
@property(atomic, readonly) BOOL isLive;

/** Whether the video will display any closed captions present in the asset. */
@property(atomic) BOOL closedCaptions;

/** The seekable range of the current stream. */
@property(atomic, readonly) ShakaBufferedRange *seekRange;

/** A list of the audio languages of the current Period. */
@property(atomic, readonly) NSArray<ShakaLanguageRole *> *audioLanguagesAndRoles;

/** A list of the text languages of the current Period. */
@property(atomic, readonly) NSArray<ShakaLanguageRole *> *textLanguagesAndRoles;

/** The buffered range of the current stream. */
@property(atomic, readonly) ShakaBufferedInfo *bufferedInfo;


/** Return playback and adaptation stats. */
- (ShakaStats *)getStats;

/**
 * Return a list of text tracks available for the current Period. If there are
 * multiple Periods, then you must seek to the Period before being able to
 * switch.
 */
- (NSArray<ShakaTrack *> *)getTextTracks;

/**
 * Return a list of variant tracks available for the current Period. If there
 * are multiple Periods, then you must seek to the Period before being able to
 * switch.
 */
- (NSArray<ShakaTrack *> *)getVariantTracks;


/**
 * Load the given manifest.
 *
 * @param uri The uri of the manifest to load.
 */
- (void)load:(NSString *)uri;

/**
 * Load the given manifest.
 *
 * @param uri The uri of the manifest to load.
 * @param startTime The time to start playing at, in seconds.
 */
- (void)load:(NSString *)uri withStartTime:(double)startTime;

/**
 * Load the given manifest asynchronously.
 *
 * @param uri The uri of the manifest to load.
 * @param block A block called when the operation is complete.
 */
- (void)load:(NSString *)uri withBlock:(ShakaPlayerAsyncBlock)block;

/**
 * Load the given manifest asynchronously.
 *
 * @param uri The uri of the manifest to load.
 * @param startTime The time to start playing at, in seconds.
 * @param block A block called when the operation is complete.
 */
- (void)load:(NSString *)uri
withStartTime:(double)startTime
     andBlock:(ShakaPlayerAsyncBlock)block;

/** Unload the current manifest and make the Player available for re-use. */
- (void)unload;

// TODO: async vesion of unload, using blocks

/**
 * Applies a configuration.
 *
 * @param namePath The path of the parameter to configure.
 *   I.e. @"manifest.dash.defaultPresentationDelay" corresponds to
 *   {manifest: {dash: {defaultPresentationDelay: *your value*}}}
 * @param value The value you wish to assign.
 */
- (void)configure:(const NSString *)namePath withBool:(BOOL)value;

/**
 * Applies a configuration.
 *
 * @param namePath The path of the parameter to configure.
 *   I.e. @"manifest.dash.defaultPresentationDelay" corresponds to
 *   {manifest: {dash: {defaultPresentationDelay: *your value*}}}
 * @param value The value you wish to assign.
 */
- (void)configure:(const NSString *)namePath withDouble:(double)value;

/**
 * Applies a configuration.
 *
 * @param namePath The path of the parameter to configure.
 *   I.e. @"manifest.dash.defaultPresentationDelay" corresponds to
 *   {manifest: {dash: {defaultPresentationDelay: *your value*}}}
 * @param value The value you wish to assign.
 */
- (void)configure:(const NSString *)namePath withString:(const NSString *)value;

/**
 * Returns a configuration to the default value.
 *
 * @param namePath The path of the parameter to configure.
 *   I.e. @"manifest.dash.defaultPresentationDelay" corresponds to
 *   {manifest: {dash: {defaultPresentationDelay: *your value*}}}
 */
- (void)configureWithDefault:(const NSString *)namePath;

/**
 * Get a configuration.
 *
 * @param namePath The path of the parameter to fetch.
 *   I.e. @"manifest.dash.defaultPresentationDelay" corresponds to
 *   {manifest: {dash: {defaultPresentationDelay: *your value*}}}
 */
- (BOOL)getConfigurationBool:(const NSString *)namePath;

/**
 * Get a configuration.
 *
 * @param namePath The path of the parameter to fetch.
 *   I.e. @"manifest.dash.defaultPresentationDelay" corresponds to
 *   {manifest: {dash: {defaultPresentationDelay: *your value*}}}
 */
- (double)getConfigurationDouble:(const NSString *)namePath;

/**
 * Get a configuration.
 *
 * @param namePath The path of the parameter to fetch.
 *   I.e. @"manifest.dash.defaultPresentationDelay" corresponds to
 *   {manifest: {dash: {defaultPresentationDelay: *your value*}}}
 */
- (NSString *)getConfigurationString:(const NSString *)namePath;


/**
 * Sets currentAudioLanguage and currentVariantRole to the selected language
 * and role, and chooses a new variant if need be.
 */
- (void)selectAudioLanguage:(NSString *)language withRole:(NSString *)role;

/**
 * Sets currentAudioLanguage to the selected language and role, and chooses a
 * new variant if need be.
 */
- (void)selectAudioLanguage:(NSString *)language;

/**
 * Sets currentTextLanguage and currentTextRole to the selected language and
 * role, and chooses a new text stream if need be.
 */
- (void)selectTextLanguage:(NSString *)language withRole:(NSString *)role;

/**
 * Sets currentTextLanguage to the selected language and role, and chooses a new
 * text stream if need be.
 */
- (void)selectTextLanguage:(NSString *)language;

/**
 * Select a specific text track. Note that AdaptationEvents are not fired for
 * manual track selections.
 */
- (void)selectTextTrack:(const ShakaTrack *)track;

/**
 * Select a specific track. Note that AdaptationEvents are not fired for
 * manual track selections.
 */
- (void)selectVariantTrack:(const ShakaTrack *)track;

/**
 * Select a specific track. Note that AdaptationEvents are not fired for
 * manual track selections.
 */
- (void)selectVariantTrack:(const ShakaTrack *)track withClearBuffer:(BOOL)clear;

/**
 * Destroys the Shaka Player instance. After calling this, this view should be
 * immediately disposed of.
 */
- (void)destroy;

@end

#endif  // SHAKA_EMBEDDED_SHAKA_PLAYER_VIEW_H_
