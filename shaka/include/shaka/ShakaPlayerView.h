// Copyright 2020 Google LLC
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

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

#include "ShakaPlayer.h"
#include "macros.h"

/**
 * A view that displays the video frames from an ShakaPlayer object.
 *
 * @ingroup player
 */
SHAKA_EXPORT
@interface ShakaPlayerView : UIView

- (id)init;

- (id)initWithPlayer:(ShakaPlayer *)player;

/** Gets/sets the ShakaPlayer instance to draw. */
@property(atomic) ShakaPlayer *player;

/** Sets how to resize the video frame within the view. */
- (void)setVideoGravity:(AVLayerVideoGravity)videoGravity;

@end

#endif  // SHAKA_EMBEDDED_SHAKA_PLAYER_VIEW_H_
