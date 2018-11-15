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

#import "ErrorDisplayView.h"
#import "NSObject+ShakaLayoutHelpers.h"

@interface ErrorDisplayView()

@property UILabel *errorLabel;
@property UIButton *hideButton;

@end

@implementation ErrorDisplayView

- (void)applyError:(NSString *)error {
  self.hidden = NO;

  if (!self.errorLabel) {
    [self setup];
  }

  self.errorLabel.text = error;
}

- (void)setup {
  self.backgroundColor = [UIColor redColor];

  self.errorLabel = [[UILabel alloc] init];
  self.errorLabel.numberOfLines = 0; // Special value meaning "as many lines as necessary".
  self.errorLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  self.errorLabel.textColor = [UIColor whiteColor];
  [self addSubview:self.errorLabel];

  UIView *controlsView = [[UIView alloc] init];
  [self addSubview:controlsView];

  self.hideButton = [[UIButton alloc] init];
  UIImage *closeIcon = [UIImage imageNamed:@"baseline_close_white_18pt"];
  [self.hideButton setBackgroundImage:closeIcon forState:UIControlStateNormal];
  self.hideButton.tintColor = [UIColor whiteColor];
  [self.hideButton addTarget:self
                      action:@selector(pressedHideButton:)
            forControlEvents:UIControlEventTouchDown];
  [controlsView addSubview:self.hideButton];

  UIView *vSpacer = [[UIView alloc] init];
  [controlsView addSubview:vSpacer];

  // TODO: Add a button for navigating to relevant docs in a browser (below close button?).

  // Layout subviews using Visual Format Language.
  NSDictionary *views = @{
    @"l" : self.errorLabel,
    @"h" : self.hideButton,
    @"c" : controlsView,
    @"s" : vSpacer
  };
  // Give the x position constraint a lower priority, so that the (external) constraint that limits
  // this view to being the width of the screen takes priority.
  [self shaka_constraint:@"|-0-[l]-0-[c]-0-|" withPriority:300.0 onViews:views];
  [self shaka_constraint:@"|-G-[h(==B)]-G-|" onViews:views];
  [self shaka_constraint:@"|-[s]-|" onViews:views];
  [self shaka_constraint:@"V:|-0-[l]-0-|" onViews:views];
  [self shaka_constraint:@"V:|-0-[c]-0-|" onViews:views];
  [self shaka_constraint:@"V:|-G-[h(==B)]-G-[s]-|" onViews:views];
}

- (void)pressedHideButton:(UIButton *)sender {
  self.hidden = YES;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  if (self.errorLabel) {
    // Update the preferredMaxLayoutWidth of the label, so that it wraps to the appropriate width.
    self.errorLabel.preferredMaxLayoutWidth =
        self.bounds.size.width - 2 * ShakaSpacing - ShakaButtonSize;
  }
}

@end
