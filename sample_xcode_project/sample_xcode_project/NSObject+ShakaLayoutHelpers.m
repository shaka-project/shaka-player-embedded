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

#import "NSObject+ShakaLayoutHelpers.h"

@implementation NSObject (ShakaLayoutHelpers)

- (NSArray<NSLayoutConstraint *> *)shaka_constraint:(NSString *)format
                                            onViews:(NSDictionary<NSString *, UIView *> *)views {
  return [self shaka_constraint:format withPriority:500.0 onViews:views];
}

- (NSArray<NSLayoutConstraint *> *)shaka_constraint:(NSString *)format
                                       withPriority:(UILayoutPriority)priority
                                            onViews:(NSDictionary<NSString *, UIView *> *)views {
  for (UIView *view in views.allValues) {
    view.translatesAutoresizingMaskIntoConstraints = NO;
  }
  // Create constraints from the provided Visual Format Language string.
  NSDictionary *metrics = @{@"G" : @(ShakaSpacing), @"B" : @(ShakaButtonSize)};
  NSArray *constraints =
      [NSLayoutConstraint constraintsWithVisualFormat:format options:0 metrics:metrics views:views];
  for (NSLayoutConstraint *constraint in constraints) {
    constraint.priority = priority;
  }
  [NSLayoutConstraint activateConstraints:constraints];
  return constraints;
}

- (NSLayoutConstraint *)shaka_equalConstraintForAttribute:(NSLayoutAttribute)attribute
                                                 fromItem:(UIView *)fromItem
                                                   toItem:(UIView *)toItem {
  return [self shaka_equalConstraintForAttribute:attribute
                                        fromItem:fromItem
                                          toItem:toItem
                                    withPriority:500.0];
}

- (NSLayoutConstraint *)shaka_equalConstraintForAttribute:(NSLayoutAttribute)attribute
                                                 fromItem:(UIView *)fromItem
                                                   toItem:(UIView *)toItem
                                             withPriority:(UILayoutPriority)priority {
  return [self shaka_relationalConstraintForAttribute:attribute
                                             fromItem:fromItem
                                               toItem:toItem
                                         withRelation:NSLayoutRelationEqual
                                         withPriority:priority];
}

- (NSLayoutConstraint *)shaka_relationalConstraintForAttribute:(NSLayoutAttribute)attribute
                                                      fromItem:(UIView *)fromItem
                                                        toItem:(UIView *)toItem
                                                  withRelation:(NSLayoutRelation)relation
                                                  withPriority:(UILayoutPriority)priority {
  NSLayoutConstraint *center = [NSLayoutConstraint constraintWithItem:fromItem
                                                            attribute:attribute
                                                            relatedBy:relation
                                                              toItem:toItem
                                                            attribute:attribute
                                                          multiplier:1
                                                            constant:0];
  center.priority = priority;
  [toItem addConstraint:center];
  return center;
}

@end
