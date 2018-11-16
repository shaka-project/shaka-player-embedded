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

#import <UIKit/UIKit.h>

typedef void (^ProgressAndBufferedSliderActiveChangedBlock)(BOOL);

@interface ProgressAndBufferedSlider : UIControl

/** Whether the slider is currently being interacted with. */
@property BOOL active;

/** Whether the presentation is live or not. */
@property BOOL isLive;

/** A block that is called whenever active changes. */
@property ProgressAndBufferedSliderActiveChangedBlock activeChangedBlock;

/** The main (progress) value of the control. */
@property (readonly) double value;

/** Sets the start and duration of the presentation. */
- (void)setStart:(double)start andDuration:(double)duration;

/** Sets the state of the sub-sliders as appropriate, based on the video's state. */
- (void)setProgress:(double)progress
      bufferedStart:(double)bufferedStart
     andBufferedEnd:(double)bufferedEnd;

/** Synchronizes the sub-sliders to the state of the nub. Meant to be used while seeking. */
- (void)synchronize;

@end
