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

#import "ProgressAndBufferedSlider.h"
#import "NSObject+ShakaLayoutHelpers.h"

@interface ProgressAndBufferedSlider() {
  BOOL _active;
}

@property UISlider *bufferedStartSlider;
@property UISlider *progressSliderNub;
@property UISlider *bufferedEndSlider;
@property UILabel *timeLabel;
@property BOOL shortDurationLivestream;
@property double minimumValue;

@end

@implementation ProgressAndBufferedSlider

- (instancetype)init {
  if (self = [super init]) {
    [self setup];
  }
  return self;
}

- (void)setStart:(double)start andDuration:(double)duration {
  NSArray <UISlider *> *sliders = @[self.progressSliderNub, self.bufferedStartSlider,
                                    self.bufferedEndSlider];
  self.shortDurationLivestream = duration < 1 && _isLive;
  self.minimumValue = self.shortDurationLivestream ? 0 : start;

  // If the duration is too low to seek, just disable interaction with the slider.
  self.progressSliderNub.userInteractionEnabled = !self.shortDurationLivestream;

  for (UISlider *slider in sliders) {
    if (self.shortDurationLivestream) {
      // Give the slider a very short duration; otherwise, it will just show the value.
      slider.maximumValue = 1;
    } else {
      slider.maximumValue = duration;
    }

    // Don't register taps until the duration has been set; otherwise, it might seek to a bad
    // position.
    if (self.progressSliderNub.gestureRecognizers.count == 0) {
      UITapGestureRecognizer *tap = [UITapGestureRecognizer alloc];
      tap = [tap initWithTarget:self action:@selector(progressSliderTap:)];
      [self.progressSliderNub addGestureRecognizer:tap];
    }
  }
}

- (void)setProgress:(double)progress
      bufferedStart:(double)bufferedStart
     andBufferedEnd:(double)bufferedEnd {
  if (self.shortDurationLivestream) {
    self.progressSliderNub.value = 1;
    self.bufferedStartSlider.value = 0;
    self.bufferedEndSlider.value = 1;
    [self setTimeLabelWithProgress:1 ifSeeking:NO];
  } else {
    self.progressSliderNub.value = progress - self.minimumValue;
    self.bufferedStartSlider.value = bufferedStart - self.minimumValue;
    self.bufferedEndSlider.value = bufferedEnd - self.minimumValue;
    [self setTimeLabelWithProgress:(progress - self.minimumValue) ifSeeking:NO];
  }
}

- (void)synchronize {
  self.bufferedStartSlider.value = self.progressSliderNub.value;
  self.bufferedEndSlider.value = self.progressSliderNub.value;
  [self setTimeLabelWithProgress:self.progressSliderNub.value ifSeeking:YES];
}

- (NSString *)formatTimeForAccessibility:(CGFloat)time {
  NSDateComponentsFormatter *formatter = [[NSDateComponentsFormatter alloc] init];
  formatter.unitsStyle = NSDateComponentsFormatterUnitsStyleFull;
  formatter.allowedUnits = NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond;
  return [formatter stringFromTimeInterval:ABS(time)];
}

- (NSString *)formatTimeForLabel:(CGFloat)time {
  NSDateComponentsFormatter *formatter = [[NSDateComponentsFormatter alloc] init];
  // Change behavior if it's before or after one hour. This is so that it will always show minutes,
  // but won't show hours until an hour has passed.
  if (time < 3600) {
    formatter.allowedUnits = NSCalendarUnitMinute | NSCalendarUnitSecond;
    formatter.zeroFormattingBehavior = NSDateComponentsFormatterZeroFormattingBehaviorPad;
  } else {
    formatter.allowedUnits = NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond;
  }
  NSString *formatted = [formatter stringFromTimeInterval:ABS(time)];
  return (time < 0) ? [NSString stringWithFormat:@"-%@", formatted] : formatted;
};

- (void)setTimeLabelWithProgress:(CGFloat)progress ifSeeking:(BOOL)seeking {
  if (self.isLive) {
    CGFloat edge = self.progressSliderNub.maximumValue;
    if (!seeking && ABS(progress - edge) < 1) {
      self.timeLabel.text = @"LIVE";
      self.accessibilityValue = @"live";
    } else {
      self.timeLabel.text = [self formatTimeForLabel:(progress - edge)];
      NSString *accessibleTime = [self formatTimeForAccessibility:(progress - edge)];
      self.accessibilityValue = [NSString stringWithFormat:@"%@ from live", accessibleTime];
    }
  } else {
    self.timeLabel.text = [self formatTimeForLabel:progress];
    self.accessibilityValue = [self formatTimeForAccessibility:progress];
  }
}

- (double)value {
  return self.progressSliderNub.value + self.minimumValue;
}

- (void)setup {
  self.active = NO;

  // Make the progress slider.
  // This is actually multiple UISlider objects on top of each other, since the vanilla UISlider can
  // only have two "tracks", while we need three (before thumb, buffered range, after buffered
  // range).
  UIImage *clearImage = [[UIImage alloc] init];

  // The "back" slider is just a solid-color backer with no thumb.
  UISlider *backer = [[UISlider alloc] init];
  backer.minimumTrackTintColor = [UIColor lightGrayColor];
  backer.maximumTrackTintColor = [UIColor lightGrayColor];
  [backer setThumbImage:clearImage forState:UIControlStateNormal];
  backer.maximumValue = 1;
  backer.userInteractionEnabled = NO;
  backer.value = 0;
  [self addSubview:backer];

  // The "lower middle" slider displays the progress.
  self.bufferedStartSlider = [[UISlider alloc] init];
  [self.bufferedStartSlider setMinimumTrackImage:clearImage forState:UIControlStateNormal];
  self.bufferedStartSlider.maximumTrackTintColor = [UIColor whiteColor];
  [self.bufferedStartSlider setThumbImage:clearImage forState:UIControlStateNormal];
  self.bufferedStartSlider.maximumValue = 1;
  self.bufferedStartSlider.userInteractionEnabled = NO;
  self.bufferedStartSlider.value = 0;
  [self addSubview:self.bufferedStartSlider];

  // The "middle" slider displays the space past the end of the buffered range.
  self.bufferedEndSlider = [[UISlider alloc] init];
  [self.bufferedEndSlider setMinimumTrackImage:clearImage forState:UIControlStateNormal];
  self.bufferedEndSlider.maximumTrackTintColor = [UIColor darkGrayColor];
  [self.bufferedEndSlider setThumbImage:clearImage forState:UIControlStateNormal];
  self.bufferedEndSlider.maximumValue = 1;
  self.bufferedEndSlider.userInteractionEnabled = NO;
  [self addSubview:self.bufferedEndSlider];

  // The "front" slider consists only of a nub. This is the element that the user controls.
  // It is separate from the progress slider so that the buffered range doesn't "cut" into it.
  self.progressSliderNub = [[UISlider alloc] init];
  [self.progressSliderNub setMinimumTrackImage:clearImage forState:UIControlStateNormal];
  [self.progressSliderNub setMaximumTrackImage:clearImage forState:UIControlStateNormal];
  self.progressSliderNub.maximumValue = 1;
  self.progressSliderNub.continuous = NO;  // If seeking was continuous, it'd waste bandwidth.
  self.progressSliderNub.value = 0;
  [self.progressSliderNub addTarget:self
                             action:@selector(progressSliderAction:)
                   forControlEvents:UIControlEventValueChanged];
  [self.progressSliderNub addTarget:self
                             action:@selector(progressSliderActivate:)
                   forControlEvents:UIControlEventTouchDown];
  [self.progressSliderNub addTarget:self
                             action:@selector(progressSliderDeactivate:)
                   forControlEvents:UIControlEventTouchCancel];
  [self addSubview:self.progressSliderNub];

  // The time label exists at the side.
  self.timeLabel = [[UILabel alloc] init];
  self.timeLabel.textColor = [UIColor whiteColor];
  self.timeLabel.textAlignment = NSTextAlignmentRight;
  [self addSubview:self.timeLabel];

  // Place autolayout constraints on views.
  NSDictionary<NSString *, UIView *> *views = @{
    @"k" : backer,
    @"b" : self.bufferedEndSlider,
    @"g" : self.bufferedStartSlider,
    @"n" : self.progressSliderNub,
    @"t" : self.timeLabel,
  };

  [self shaka_constraint:@"|-0-[k]-G-[t]" onViews:views];
  [self shaka_constraint:@"|-0-[g]-G-[t]" onViews:views];
  [self shaka_constraint:@"|-0-[b]-G-[t]" onViews:views];
  [self shaka_constraint:@"|-0-[n]-G-[t]" onViews:views];
  [self shaka_constraint:@"[t(==40)]-0-|" onViews:views];

  // Add constraints to ensure that the layers of the progress slider properly overlap each other.
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterY
                                 fromItem:backer
                                   toItem:self];
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterY
                                 fromItem:self.bufferedStartSlider
                                   toItem:self];
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterY
                                 fromItem:self.bufferedEndSlider
                                   toItem:self];
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterY
                                 fromItem:self.progressSliderNub
                                   toItem:self];
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterY
                                 fromItem:self.timeLabel
                                   toItem:self];

  // Set accessibility properties. This should be a single element to VoiceOver, so it can't just
  // use the accessibility properties of its sub-sliders.
  self.isAccessibilityElement = YES;
  self.accessibilityLabel = @"seek slider";
  self.accessibilityTraits = UIAccessibilityTraitAdjustable;
}

- (void)accessibilityIncrement {
  [self.progressSliderNub accessibilityIncrement];
}

- (void)accessibilityDecrement {
  [self.progressSliderNub accessibilityDecrement];
}

- (void)progressSliderAction:(UISlider *)sender {
  self.active = NO;
  [self sendActionsForControlEvents:UIControlEventValueChanged];
}

- (void)progressSliderActivate:(UISlider *)sender {
  self.active = YES;
}

- (void)progressSliderDeactivate:(UISlider *)sender {
  self.active = NO;
}

- (void)progressSliderTap:(UIGestureRecognizer *)sender {
  if (!self.progressSliderNub.userInteractionEnabled) {
    // Don't seek if user interaction is disabled.
    return;
  }

  CGPoint point = [sender locationInView:self.progressSliderNub];
  CGFloat percent = point.x / self.progressSliderNub.bounds.size.width;
  self.progressSliderNub.value = self.progressSliderNub.maximumValue * percent;

  self.active = NO;
  [self sendActionsForControlEvents:UIControlEventValueChanged];
}

- (void)setActive:(BOOL)active {
  BOOL activeChanged = _active != active;
  _active = active;
  if (self.activeChangedBlock && activeChanged)
    self.activeChangedBlock(_active);
}

- (BOOL)active {
  return _active;
}

@end
