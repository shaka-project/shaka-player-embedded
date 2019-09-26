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
#import "PlayerViewController.h"
#import "ProgressAndBufferedSlider.h"
#import "NSObject+ShakaLayoutHelpers.h"
#import <ShakaPlayerEmbedded/ShakaPlayerEmbedded.h>

#define ShakaFadeBegin 4
#define ShakaFadeEnd 4.25
#define ShakaMinWidthForButtons 450

typedef enum { kPlayPauseIconPlay, kPlayPauseIconPause, kPlayPauseIconReplay } PlayPauseIcon;

@interface PlayerViewController () <ShakaPlayerClient>

@property UIView *controlsView;
@property UIView *audioOnlyView;
@property UIButton *playPauseButton;
@property UIButton *textLanguageButton;
@property UIButton *audioLanguageButton;
@property UIButton *detailDisclosureButton;
@property ProgressAndBufferedSlider *progressSlider;
@property NSTimer *uiTimer;
@property ErrorDisplayView *errorDisplayView;
@property PlayPauseIcon oldPlayPauseIcon;
@property BOOL oldClosedCaptions;
@property UIView *playerContainer;
@property ShakaPlayerView *player;
@property CFTimeInterval lastActionTime;
@property NSLayoutConstraint *navigationBarHeightConstraint;
@property UIActivityIndicatorView *spinner;
@property NSArray<NSLayoutConstraint *> *wideSpacingConstraints;
@property NSArray<NSLayoutConstraint *> *wideWidthConstraints;
@property BOOL wasPausedBeforeSeeking;
@property BOOL wasPausedBeforeHalting;
@property double timeBeforeHalting;

@end

@implementation PlayerViewController

+ (NSString *)getShakaPlayerVersion {
  return [[NSString alloc] initWithUTF8String:GetShakaEmbeddedVersion()];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];

  // Enable sleep again, so that it can go to sleep while selecting assets.
  [UIApplication sharedApplication].idleTimerDisabled = NO;

  // Un-listen to VoiceOver notifications.
  NSNotificationName voiceOverNotification = UIAccessibilityVoiceOverStatusDidChangeNotification;
  [[NSNotificationCenter defaultCenter] removeObserver:self name:voiceOverNotification object:nil];

  // Un-listen to enter foreground/background notifications.
  [[NSNotificationCenter defaultCenter] removeObserver:self
                                                  name:UIApplicationWillResignActiveNotification
                                                object:nil];
  [[NSNotificationCenter defaultCenter] removeObserver:self
                                                  name:UIApplicationWillEnterForegroundNotification
                                                object:nil];

  [self.uiTimer invalidate];

  [self.player unloadWithBlock:^(ShakaPlayerError* error) {
    [self.player removeFromSuperview];
    self.player = nil;
  }];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Don't go to sleep while watching a video.
  [UIApplication sharedApplication].idleTimerDisabled = YES;

  // Listen to VoiceOver notifications.
  NSNotificationName voiceOverNotification = UIAccessibilityVoiceOverStatusDidChangeNotification;
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(voiceOverStatusChanged)
                                               name:voiceOverNotification
                                             object:nil];

  // Listen to enter foreground/background notifications.
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(enterBackground:)
                                               name:UIApplicationWillResignActiveNotification
                                             object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(leaveBackground:)
                                               name:UIApplicationWillEnterForegroundNotification
                                             object:nil];

  // Keep views updated.
  self.lastActionTime = CACurrentMediaTime();
  [self updateViews];
  [self startUITimer];
}

- (void)startUITimer {
  __weak PlayerViewController *weakSelf = self;
  self.uiTimer = [NSTimer scheduledTimerWithTimeInterval:0.05
                                                 repeats:YES
                                                   block:^(NSTimer *_Nonnull timer) {
                                                     [weakSelf updateViews];
                                                   }];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // Dispatch asynchronously, so that the view can finish transitioning while the player is loading.
  // The player doesn't load correctly when not on the main queue, so this is just dispatching back
  // to the main queue.
  dispatch_async(dispatch_get_main_queue(), ^{
    // Check to make sure you weren't unloaded in the meantime.
    if (self.parentViewController) {
      [self configureAndLoadAssetWithAutoplay:YES andStartTime:NAN];
    }
  });
}

- (void)configureAndLoadAssetWithAutoplay:(BOOL)autoplay andStartTime:(double)startTime {
  if (self.configuration) {
    for (NSString *key in self.configuration) {
      NSObject *value = self.configuration[key];
      if ([value isKindOfClass:[NSString class]]) {
        NSString *stringValue = (NSString *)value;
        [self.player configure:key withString:stringValue];
      } else if ([value isKindOfClass:[NSNumber class]]) {
        // TODO: Detect if this is a bool, or actually a number.
        NSNumber *numberValue = (NSNumber *)value;
        [self.player configure:key withBool:numberValue.boolValue];
      }
    }
  }
  [self.player load:self.assetURI withStartTime:startTime andBlock:^(ShakaPlayerError *error){
    if (error) {
      [self.errorDisplayView applyError:[error message]];
    } else {
      if (autoplay) {
        [self.player play];
      }
      if (self.player.isAudioOnly) {
        self.audioOnlyView.hidden = NO;
        self.player.backgroundColor = [UIColor whiteColor];
      }
    }
  }];
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];

  // Memory is running out! Try to minimize memory usage from here on out.

  // Narrow the buffered window.
  [self.player configure:kBufferingGoal withDouble:5.0];
  [self.player configure:kRebufferingGoal withDouble:1.0];
  [self.player configure:kBufferBehind withDouble:1.0];

  // Restrict the maximum size, so that the player will stop playing high-resolution variants.
  // TODO: If there is no variant small enough, this will cause the player to crash. Determine
  // that beforehand.
  [self.player configure:kMaxHeight withDouble:480.0];

  // TODO: Maybe implement the proposed "memory-pressure" web event (proposal
  // here: https://github.com/WICG/memory-pressure), and dispatch an event from this
  // to be handled at the JS level.
}

- (BOOL)prefersStatusBarHidden {
  // Because the battery bar looks weird on a black background.
  return YES;
}

- (void)enterBackground:(NSNotification *)note {
  self.wasPausedBeforeHalting = self.player.paused;
  self.timeBeforeHalting = self.player.currentTime;
  // TODO: Remember changes to text/audio track, CC state, etc.
  [self.uiTimer invalidate];
  self.uiTimer = nil;
  [self.player destroy];
  [self.player removeFromSuperview];
  self.player = nil;
}

- (void)leaveBackground:(NSNotification *)note {
  [self placePlayerIntoContainer];
  // TODO: Re-apply changes to text/audio track, CC state, etc.
  [self configureAndLoadAssetWithAutoplay:!self.wasPausedBeforeHalting
                             andStartTime:self.timeBeforeHalting];
  [self startUITimer];
}

#pragma mark - ShakaPlayerClient

- (void)onPlayerError:(ShakaPlayerError *)error {
  [self.errorDisplayView applyError:[error message]];
}

- (void)onPlayerBufferingChange:(BOOL)is_buffering {
  if (is_buffering) {
    [self.spinner startAnimating];
  } else {
    [self.spinner stopAnimating];
  }
}

#pragma mark - updating

- (UIImage *)getIcon:(NSString *)name {
  NSString *fullName = [NSString stringWithFormat:@"%@_white_18pt", name];
  UIImage *image = [UIImage imageNamed:fullName];
  image = [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  return image;
}

- (void)setIcon:(NSString *)name withColor:(UIColor *)color forButton:(UIButton *)button {
  [button setBackgroundImage:[self getIcon:name] forState:UIControlStateNormal];
  button.tintColor = color;
}

- (CGFloat)navBarHeight {
  CGRect navBarFrame = self.navigationController.navigationBar.frame;
  return navBarFrame.size.height + navBarFrame.origin.y;
}

- (void)updateViews {
  if (!self.player || !self.player.superview) [self setupTopLevelViews];

  if (self.oldPlayPauseIcon != self.playPauseIcon) [self updatePlayPauseButton];
  if (self.player.closedCaptions != self.oldClosedCaptions) [self updateClosedCaptionsButton];

  CFTimeInterval currentTime = CACurrentMediaTime();

  // Set live edge of presentation.
  ShakaBufferedRange *seekable = self.player.seekRange;
  [self.progressSlider setStart:seekable.start andDuration:(seekable.end - seekable.start)];

  if (self.progressSlider.active || self.player.paused || UIAccessibilityIsVoiceOverRunning()) {
    // While a slider is being used, VoiceOver is on, or the player is paused, don't fade.
    self.lastActionTime = currentTime;
  }

  if (!self.errorDisplayView.hidden) {
    // If there's an error being displayed, don't fade.
    self.lastActionTime = currentTime;
  }

  // Keep track of the height of the navigation bar manually.
  self.navigationBarHeightConstraint.constant = self.navBarHeight;

  // Make the controls fade away if they haven't been used recently.
  CFTimeInterval elapsed = currentTime - self.lastActionTime;
  if (elapsed > ShakaFadeEnd) {
    self.controlsView.hidden = YES;
    self.navigationController.navigationBar.hidden = YES;
    return;
  } else if (elapsed > ShakaFadeBegin) {
    CGFloat alpha = 1 - (elapsed - ShakaFadeBegin) / (ShakaFadeEnd - ShakaFadeBegin);
    self.controlsView.alpha = alpha;
    self.navigationController.navigationBar.alpha = alpha;
  } else {
    self.controlsView.alpha = 1.0;
    self.navigationController.navigationBar.alpha = 1.0;
  }
  self.controlsView.hidden = NO;
  self.navigationController.navigationBar.hidden = NO;

  // Don't update the progress slider while the user is controlling it, or while the player is
  // seeking.
  if (self.player.seeking || self.progressSlider.active) {
    // Keep up with the player's input on the nub.
    [self.progressSlider synchronize];
  } else {
    self.progressSlider.isLive = self.player.isLive;

    CGFloat progress = 0;
    CGFloat bufferedStart = 0;
    CGFloat bufferedEnd = 0;
    // The durations is nan before the asset is loaded; in that situation, the progress should be 0.
    if (!isnan(self.player.duration)) {
      NSArray<ShakaBufferedRange *> *bufferedRanges = self.player.bufferedInfo.total;
      progress = self.player.currentTime;
      if (bufferedRanges.count) {
        ShakaBufferedRange *buffered = bufferedRanges[0];
        bufferedStart = buffered.start;
        bufferedEnd = buffered.end;
      }
    }
    [self.progressSlider setProgress:progress
                       bufferedStart:bufferedStart
                      andBufferedEnd:bufferedEnd];
  }
}

- (void)updatePlayPauseButton {
  NSString *iconName;
  switch (self.playPauseIcon) {
    case kPlayPauseIconPlay:
      self.playPauseButton.accessibilityLabel = @"play button";
      iconName = @"ic_play_arrow";
      break;
    case kPlayPauseIconPause:
      self.playPauseButton.accessibilityLabel = @"pause button";
      iconName = @"ic_pause";
      break;
    case kPlayPauseIconReplay:
      self.playPauseButton.accessibilityLabel = @"replay button";
      iconName = @"ic_replay";
      break;
  }
  [self setIcon:iconName withColor:[UIColor whiteColor] forButton:self.playPauseButton];
  self.oldPlayPauseIcon = self.playPauseIcon;
}

- (PlayPauseIcon)playPauseIcon {
  if (self.player.ended)
    return kPlayPauseIconReplay;
  else if (self.player.paused)
    return kPlayPauseIconPlay;
  return kPlayPauseIconPause;
}

- (void)updateClosedCaptionsButton {
  [self setIcon:@"ic_closed_caption"
      withColor:(self.player.closedCaptions ? [UIColor whiteColor] : [UIColor darkGrayColor])
      forButton:self.textLanguageButton];
  self.oldClosedCaptions = self.player.closedCaptions;
}

#pragma mark - layout

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
  [super touchesBegan:touches withEvent:event];
  self.lastActionTime = CACurrentMediaTime();
}

- (void)placePlayerIntoContainer {
  if (!self.player)
    self.player = [[ShakaPlayerView alloc] initWithClient:self];
  self.player.backgroundColor = [UIColor blackColor];
  [self.playerContainer addSubview:self.player];

  // Place autolayout constraints on views, using Visual Format Language.
  NSDictionary<NSString *, UIView *> *views = @{@"p" : self.player};
  [self shaka_constraint:@"|-0-[p]-0-|" onViews:views];
  [self shaka_constraint:@"V:|-0-[p]-0-|" onViews:views];
}

- (void)setupTopLevelViews {
  self.playerContainer = [[UIView alloc] init];
  self.playerContainer.isAccessibilityElement = NO;
  [self.view addSubview:self.playerContainer];

  [self placePlayerIntoContainer];

  NSMutableArray <UIImage *> *audioOnlyFrames = [[NSMutableArray alloc] init];
  for (int i = 1; i <= 4; i++) {
    NSString *imageName = [NSString stringWithFormat:@"audio_only_%i", i];
    UIImage *image = [UIImage imageNamed:imageName];
    // Every other frame lasts three times as long.
    int duration = (i % 2 == 0) ? 3 : 1;
    for (int j = 0; j < duration; j++) {
      [audioOnlyFrames addObject:image];
    }
  }
  UIImage *audioOnlyPoster = [UIImage animatedImageWithImages:audioOnlyFrames duration:1.0f];
  self.audioOnlyView = [[UIImageView alloc] initWithImage:audioOnlyPoster];
  self.audioOnlyView.hidden = YES; // Hidden by default.
  self.audioOnlyView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.audioOnlyView];

  UIActivityIndicatorViewStyle style = UIActivityIndicatorViewStyleWhiteLarge;
  self.spinner = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:style];
  [self.spinner startAnimating];
  [self.player addSubview:self.spinner];

  self.errorDisplayView = [[ErrorDisplayView alloc] init];
  self.errorDisplayView.hidden = YES;
  [self.view addSubview:self.errorDisplayView];

  self.controlsView = [[UIView alloc] init];
  [self.view addSubview:self.controlsView];
  self.controlsView.backgroundColor = [[UIColor blackColor] colorWithAlphaComponent:0.5];
  self.controlsView.layer.cornerRadius = 10.0;

  // Place autolayout constraints on views, using Visual Format Language.
  NSDictionary<NSString *, UIView *> *views = @{
    @"t" : self.playerContainer,
    @"e" : self.errorDisplayView,
    @"r" : self.controlsView,
  };
  [self shaka_constraint:@"|-0-[t]-0-|" onViews:views];
  [self shaka_constraint:@"|-0-[e]-0-|" onViews:views];
  [self shaka_constraint:@"|-G-[r]-G-|" onViews:views];
  [self shaka_constraint:@"V:|-0-[t]-0-|" onViews:views];
  [self shaka_constraint:@"V:[r(==B)]-0-|" onViews:views];

  // Add a constraint that links the top of the error display view and the bottom of the navigation
  // bar together.
  // Unfortunately, there's no way to do this directly, as the navigation bar is not inside the view
  // hierarchy of this view controller, so it has to be done indirectly.
  self.navigationBarHeightConstraint = [self shaka_equalConstraintForAttribute:NSLayoutAttributeTop
                                                                      fromItem:self.errorDisplayView
                                                                        toItem:self.view];
  self.navigationBarHeightConstraint.constant = self.navBarHeight;
  [self.view addConstraint:self.navigationBarHeightConstraint];

  // Making a view that is as large as possible while still maintaining a given aspect ratio is a
  // complex task; this is the simplest set of constraints that can accomplish that.
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterX
                                 fromItem:self.audioOnlyView
                                   toItem:self.view];
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterY
                                 fromItem:self.audioOnlyView
                                   toItem:self.view];
  [self shaka_relationalConstraintForAttribute:NSLayoutAttributeWidth
                                      fromItem:self.audioOnlyView
                                        toItem:self.view
                                  withRelation:NSLayoutRelationLessThanOrEqual
                                  withPriority:1000.0];
  [self shaka_relationalConstraintForAttribute:NSLayoutAttributeHeight
                                      fromItem:self.audioOnlyView
                                        toItem:self.view
                                  withRelation:NSLayoutRelationLessThanOrEqual
                                  withPriority:1000.0];
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeWidth
                                 fromItem:self.audioOnlyView
                                   toItem:self.view
                             withPriority:250];
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeHeight
                                 fromItem:self.audioOnlyView
                                   toItem:self.view
                             withPriority:250];
  NSLayoutConstraint *aspectRatio = [NSLayoutConstraint constraintWithItem:self.audioOnlyView
                                                                 attribute:NSLayoutAttributeHeight
                                                                 relatedBy:NSLayoutRelationEqual
                                                                    toItem:self.audioOnlyView
                                                                 attribute:NSLayoutAttributeWidth
                                                                multiplier:0.75
                                                                  constant:0];
  aspectRatio.priority = 1000.0;
  [self.audioOnlyView addConstraint:aspectRatio];

  // Add constraints to center the spinner in the player view.
  // Unfortunately, this cannot be done using Visual Format Language.
  self.spinner.translatesAutoresizingMaskIntoConstraints = NO;
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterX
                                 fromItem:self.spinner
                                   toItem:self.player];
  [self shaka_equalConstraintForAttribute:NSLayoutAttributeCenterY
                                 fromItem:self.spinner
                                   toItem:self.player];

  [self setupControls];
}

- (void)setupControls {
  // Make views.
  self.playPauseButton = [[UIButton alloc] init];
  [self.playPauseButton addTarget:self
                           action:@selector(pressedPlayPauseButton:)
                 forControlEvents:UIControlEventTouchDown];
  [self updatePlayPauseButton];
  [self.controlsView addSubview:self.playPauseButton];

  UIView *extraButtonsView = [[UIView alloc] init];
  [self.controlsView addSubview:extraButtonsView];

  self.textLanguageButton = [[UIButton alloc] init];
  [self.textLanguageButton addTarget:self
                              action:@selector(pressedTextLanguageButton:)
                    forControlEvents:UIControlEventTouchDown];
  [self updateClosedCaptionsButton];
  self.textLanguageButton.accessibilityLabel = @"text language button";
  [extraButtonsView addSubview:self.textLanguageButton];

  self.audioLanguageButton = [[UIButton alloc] init];
  [self.audioLanguageButton addTarget:self
                               action:@selector(pressedAudioLanguageButton:)
                     forControlEvents:UIControlEventTouchDown];
  [self setIcon:@"baseline_language"
      withColor:[UIColor whiteColor]
      forButton:self.audioLanguageButton];
  self.audioLanguageButton.accessibilityLabel = @"audio language button";
  [extraButtonsView addSubview:self.audioLanguageButton];

  self.detailDisclosureButton = [[UIButton alloc] init];
  [self.detailDisclosureButton addTarget:self
                                  action:@selector(pressedDetailDisclosureButton:)
                        forControlEvents:UIControlEventTouchDown];
  [self setIcon:@"baseline_more_vert"
      withColor:[UIColor whiteColor]
      forButton:self.detailDisclosureButton];
  self.detailDisclosureButton.accessibilityElementsHidden = YES;
  [extraButtonsView addSubview:self.detailDisclosureButton];

  self.progressSlider = [[ProgressAndBufferedSlider alloc] init];
  [self.progressSlider addTarget:self
                          action:@selector(progressSliderAction:)
                forControlEvents:UIControlEventValueChanged];
  self.wasPausedBeforeSeeking = NO;
  __weak PlayerViewController *weakSelf = self;
  self.progressSlider.activeChangedBlock = ^(BOOL active) {
    // If the progress slider is being dragged, pause. Unpause at the end.
    __strong PlayerViewController *strongSelf = weakSelf;
    if (active) {
      strongSelf.wasPausedBeforeSeeking = weakSelf.player.paused;
      [strongSelf.player pause];
    } else if (!strongSelf.wasPausedBeforeSeeking) {
      [strongSelf.player play];
    }
  };
  [self.controlsView addSubview:self.progressSlider];

  // Place autolayout constraints on views.
  NSDictionary<NSString *, UIView *> *views = @{
    @"p" : self.playPauseButton,
    @"s" : self.progressSlider,
    @"d" : self.detailDisclosureButton,
    @"t" : self.textLanguageButton,
    @"a" : self.audioLanguageButton,
    @"x" : extraButtonsView,
  };
  [self shaka_constraint:@"V:|-0-[p(==B)]-0-|" onViews:views];
  [self shaka_constraint:@"V:|-G-[s]-G-|" onViews:views];
  [self shaka_constraint:@"V:|-0-[t]-0-|" onViews:views];
  [self shaka_constraint:@"V:|-0-[a]-0-|" onViews:views];
  [self shaka_constraint:@"V:|-0-[x]-0-|" onViews:views];
  [self shaka_constraint:@"V:|-0-[d]-0-|" onViews:views];
  [self shaka_constraint:@"|-G-[p(==B)]-G-[s]-G-[x]-G-|" onViews:views];
  self.wideSpacingConstraints = [self shaka_constraint:@"[a]-G-[t]" onViews:views];
  NSMutableArray <NSLayoutConstraint *> *widthConstraints = [[NSMutableArray alloc] init];
  [widthConstraints addObjectsFromArray:[self shaka_constraint:@"[a(==B)]" onViews:views]];
  [widthConstraints addObjectsFromArray:[self shaka_constraint:@"[t(==B)]" onViews:views]];
  self.wideWidthConstraints = widthConstraints;
  [self shaka_constraint:@"|-0-[a]" onViews:views];
  [self shaka_constraint:@"[t]-0-|" onViews:views];
  [self shaka_constraint:@"|-0-[d(==B)]-0-|" withPriority:250 onViews:views];

  // Simulate a rotation, in case it starts out in portrait mode.
  [self setLayoutBasedOnSize:[UIScreen mainScreen].bounds.size];
}

- (void)voiceOverStatusChanged {
  [self setLayoutBasedOnSize:[UIScreen mainScreen].bounds.size];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
  [self setLayoutBasedOnSize:size];
  self.lastActionTime = CACurrentMediaTime();
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  // Tell the navigation controller to layout the navigation bar also.
  // This is a workaround for an iOS11 bug.
  [self.navigationController.navigationBar setNeedsLayout];
}

- (void)setLayoutBasedOnSize:(CGSize)size {
  // If VoiceOver is on, treat the layout as though it were always wide. This is to keep VoiceOver
  // users from being confused by invisible layout changes from screen rotation.
  BOOL wide = size.width > ShakaMinWidthForButtons || UIAccessibilityIsVoiceOverRunning();
  self.audioLanguageButton.hidden = !wide;
  self.textLanguageButton.hidden = !wide;
  self.detailDisclosureButton.hidden = wide;
  for (NSLayoutConstraint *constraint in self.wideSpacingConstraints) {
    constraint.constant = wide ? ShakaSpacing : 0;
  }
  for (NSLayoutConstraint *constraint in self.wideWidthConstraints) {
    constraint.constant = wide ? ShakaButtonSize : 0;
  }
}

#pragma mark - controls

- (UIAlertController *)makeLanguageControllerWithTitle:(NSString *)title
                                     languagesAndRoles:(NSArray <ShakaLanguageRole *>*)languageRoles
                                      selectedLanguage:(NSString *)selectedLanguage
                                                source:(UIView *)source
                                              andBlock:(void(^)(ShakaLanguageRole *))block {
  UIAlertControllerStyle style = UIAlertControllerStyleActionSheet;
  UIAlertController *alert = [UIAlertController alertControllerWithTitle:title
                                                                 message:nil
                                                          preferredStyle:style];
  for (ShakaLanguageRole *languageRole in languageRoles) {
    BOOL selected = selectedLanguage && [selectedLanguage isEqualToString:languageRole.language];
    NSString *label = [[NSLocale currentLocale] displayNameForKey:NSLocaleIdentifier
                                                            value:languageRole.language];
    if (!label)
      label = languageRole.language;
    UIAlertAction *action = [self makeActionWithTitle:label
                                           imageNamed:nil
                                            checkMark:selected
                                             andBlock:^{
                                               block(languageRole);
                                             }];
    [alert addAction:action];
  }
  UIAlertAction *cancel = [UIAlertAction actionWithTitle:@"Cancel"
                                                   style:UIAlertActionStyleCancel
                                                 handler:nil];
  // TODO: This is an undocumented API feature and might change at any time.
  // If possible, find a better way to set color.
  [cancel setValue:[UIColor blackColor] forKey:@"titleTextColor"];
  [alert addAction:cancel];

  [self presentAlert:alert fromSource:source];
  return alert;
}

- (void)pressedTextLanguageButton:(UIButton *)sender {
  NSString *textLanguage = nil;
  if (self.player.closedCaptions) {
    for (ShakaTrack *track in [self.player getTextTracks]) {
      if (track.active) {
        textLanguage = track.language;
        break;
      }
    }
  }

  __weak PlayerViewController *weakSelf = self;
  void (^selectLanguageBlock)(ShakaLanguageRole *) = ^(ShakaLanguageRole *languageRole) {
    // Enable captions. If you are setting text language, you are
    // probably interested in seeing the captions.
    weakSelf.player.closedCaptions = YES;
    [weakSelf.player selectTextLanguage:languageRole.language];
  };
  UIAlertController *alert = [self makeLanguageControllerWithTitle:@"Text Language"
                                                 languagesAndRoles:self.player.textLanguagesAndRoles
                                                  selectedLanguage:textLanguage
                                                            source:sender
                                                          andBlock:selectLanguageBlock];
  UIAlertAction *none = [self makeActionWithTitle:@"None"
                                       imageNamed:nil
                                        checkMark:(textLanguage == nil)
                                         andBlock:^{
    // Disable closed captions.
    self.player.closedCaptions = NO;
  }];
  [alert addAction:none];
}

- (void)pressedAudioLanguageButton:(UIButton *)sender {
  NSString *audioLanguage = nil;
  for (ShakaTrack *track in [self.player getVariantTracks]) {
    if (track.active) {
      audioLanguage = track.language;
      break;
    }
  }

  __weak PlayerViewController *weakSelf = self;
  [self makeLanguageControllerWithTitle:@"Audio Language"
                      languagesAndRoles:self.player.audioLanguagesAndRoles
                       selectedLanguage:audioLanguage
                                 source:sender
                               andBlock:^(ShakaLanguageRole *languageRole) {
                                 [weakSelf.player selectAudioLanguage:languageRole.language];
                               }];
}

- (UIAlertAction *)makeActionWithTitle:(NSString *)title
                            imageNamed:(NSString *)imageName
                             checkMark:(BOOL)checkMark
                              andBlock:(void(^)(void))block {
  UIAlertAction *action = [UIAlertAction actionWithTitle:title
                                                   style:UIAlertActionStyleDefault
                                                 handler:^(UIAlertAction * _Nonnull action) {
                                                   if (block) {
                                                     block();
                                                   }
                                                 }];
  // TODO: These are an undocumented API features and might change at any time.
  // If possible, find a better way to add check marks, and set color.
  [action setValue:[self getIcon:imageName] forKey:@"image"];
  [action setValue:[UIColor blackColor] forKey:@"titleTextColor"];
  if (checkMark) {
    [action setValue:@YES forKey:@"checked"];
  }
  return action;
}

- (void)presentAlert:(UIAlertController *)alert fromSource:(UIView *)source {
  UIPopoverPresentationController *presenter = [alert popoverPresentationController];
  presenter.sourceView = source;
  presenter.sourceRect = source.bounds;
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)pressedDetailDisclosureButton:(UIButton *)sender {
  UIAlertControllerStyle style = UIAlertControllerStyleActionSheet;
  UIAlertController *alert = [UIAlertController alertControllerWithTitle:nil
                                                                 message:nil
                                                          preferredStyle:style];

  [alert addAction:[self makeActionWithTitle:@"Audio Language"
                                  imageNamed:@"baseline_language"
                                   checkMark:NO
                                    andBlock:^{
    [self pressedAudioLanguageButton:self.audioLanguageButton];
  }]];

  UIAlertAction *textAction = [self makeActionWithTitle:@"Text Language"
                                             imageNamed:@"ic_closed_caption"
                                              checkMark:NO
                                               andBlock:^{
    [self pressedTextLanguageButton:self.textLanguageButton];
  }];
  // Mark closed captions being off with a different color.
  if (!self.player.closedCaptions) {
    // TODO: This is an undocumented API feature and might change at any time.
    // If possible, find a better way to set color.
    [textAction setValue:[UIColor darkGrayColor] forKey:@"titleTextColor"];
  }
  [alert addAction:textAction];

  UIAlertAction *cancel = [UIAlertAction actionWithTitle:@"Cancel"
                                                   style:UIAlertActionStyleCancel
                                                 handler:nil];
  // TODO: This is an undocumented API feature and might change at any time.
  // If possible, find a better way to set color.
  [cancel setValue:[UIColor blackColor] forKey:@"titleTextColor"];
  [alert addAction:cancel];

  [self presentAlert:alert fromSource:self.detailDisclosureButton];
}

- (void)progressSliderAction:(UISlider *)sender {
  const BOOL ended = self.player.ended;
  self.player.currentTime = self.progressSlider.value;
  if (ended)
    [self.player play];
}

- (void)pressedPlayPauseButton:(UIButton *)sender {
  if (self.player.ended) {
    self.player.currentTime = 0;
    [self.player play];
  } else if (self.player.paused) {
    [self.player play];
  } else {
    [self.player pause];
  }
}

@end
