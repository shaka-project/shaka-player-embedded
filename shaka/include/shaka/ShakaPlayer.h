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

#ifndef SHAKA_EMBEDDED_SHAKA_PLAYER_H_
#define SHAKA_EMBEDDED_SHAKA_PLAYER_H_

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "error_objc.h"
#include "macros.h"
#include "manifest_objc.h"
#include "player_externs_objc.h"
#include "stats_objc.h"
#include "track_objc.h"

@class ShakaPlayer;

typedef void (^ShakaPlayerAsyncBlock)(ShakaPlayerError *);

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
 * Defines an interface for Player events.
 * @ingroup player
 */
SHAKA_EXPORT
@protocol ShakaPlayerClient <NSObject>

@optional

/**
 * Called when an asynchronous error occurs.  This is called on the main thread
 * and is only called when there isn't a block callback to give the error to.
 */
- (void)onPlayer:(ShakaPlayer *)player error:(ShakaPlayerError *)error;

/**
 * Called when the buffering state of the Player changes.
 */
- (void)onPlayer:(ShakaPlayer *)player bufferingChange:(BOOL)is_buffering;


/**
 * Called when the video starts playing after startup or a call to Pause().
 */
- (void)onPlayerPlayingEvent:(ShakaPlayer *)player;

/**
 * Called when the video gets paused due to a call to Pause().
 */
- (void)onPlayerPauseEvent:(ShakaPlayer *)player;

/**
 * Called when the video plays to the end of the content.
 */
- (void)onPlayerEndedEvent:(ShakaPlayer *)player;


/**
 * Called when the video starts seeking.  This may be called multiple times
 * in a row due to Shaka Player repositioning the playhead.
 */
- (void)onPlayerSeekingEvent:(ShakaPlayer *)player;

/**
 * Called when the video completes seeking.  This happens once content is
 * available and the playhead can move forward.
 */
- (void)onPlayerSeekedEvent:(ShakaPlayer *)player;

/** Called once MSE-based playback has started. */
- (void)onPlayerAttachMse:(ShakaPlayer *)player;

/**
 * Called once src= based playback has started.  Once this is called, the avPlayer property
 * will be valid and point to the AVPlayer instance being used.
 */
- (void)onPlayerAttachSource:(ShakaPlayer *)player;

/**
 * Called once playback is detached.  If this was src= playback, the AVPlayer is no longer usable.
 */
- (void)onPlayerDetach:(ShakaPlayer *)player;

@end


/**
 * Handles loading and playback of media content.  The is the control aspect of playback.  Use
 * a ShakaPlayerView to display the video frames.  This will still load and play content without
 * an active view.  This will play audio without a view.
 *
 * @ingroup player
 */
SHAKA_EXPORT
@interface ShakaPlayer : NSObject

- (instancetype)init NS_UNAVAILABLE; // initWithError: should always be used

/**
 * Creates a new initialized Player object.  If there is an error, the `error` pointer will
 * be set to an object containing the error information and this returns nil.
 */
- (instancetype)initWithError:(NSError *__autoreleasing *)error NS_SWIFT_NAME(init());


/** A client which will receive player events */
@property (atomic, weak) id<ShakaPlayerClient> client;

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

/**
 * Gets the current AVPlayer instance used to play src= content.  This is only valid after starting
 * playback of src= content.  Use the client events to detect when src= content starts.  New
 * playbacks will use a new instance.
 */
@property(atomic, readonly) AVPlayer *avPlayer;


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
- (void)unloadWithBlock:(ShakaPlayerAsyncBlock)block;


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


//@{
/**
 * Adds the given text track to the current Period.  <code>load</code> must
 * resolve before calling.  The current Period or the presentation must have a
 * duration.
 * This returns a Promise that will resolve with the track that was created,
 * when that track can be switched to.
 */
- (void)addTextTrack:(NSString *)uri
            language:(NSString *)lang
                kind:(NSString *)kind
                mime:(NSString *)mime;
- (void)addTextTrack:(NSString *)uri
            language:(NSString *)lang
                kind:(NSString *)kind
                mime:(NSString *)mime
               codec:(NSString *)codec;
- (void)addTextTrack:(NSString *)uri
            language:(NSString *)lang
                kind:(NSString *)kind
                mime:(NSString *)mime
               codec:(NSString *)codec
               label:(NSString *)label;
//@}

@end

#endif  // SHAKA_EMBEDDED_SHAKA_PLAYER_H_
