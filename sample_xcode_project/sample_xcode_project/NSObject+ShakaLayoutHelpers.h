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

#define ShakaSpacing 5
#define ShakaButtonSize 45

/** A category that adds helper methods for creating and activating NSLayoutConstraints. */
@interface NSObject (ShakaLayoutHelpers)

/** Applies constraints defined with VFL to views. */
- (NSArray<NSLayoutConstraint *> *)shaka_constraint:(NSString *)format
                                            onViews:(NSDictionary<NSString *, UIView *> *)views;

/** Applies constraints defined with VFL to views. Also applies a custom priority. */
- (NSArray<NSLayoutConstraint *> *)shaka_constraint:(NSString *)format
                                       withPriority:(UILayoutPriority)priority
                                            onViews:(NSDictionary<NSString *, UIView *> *)views;

/** Link two views together, making them have a given attribute be equal. */
- (NSLayoutConstraint *)shaka_equalConstraintForAttribute:(NSLayoutAttribute)attribute
                                                 fromItem:(UIView *)fromItem
                                                   toItem:(UIView *)toItem;

/**
 * Link two views together, making them have a given attribute be equal.
 * Also applies a custom priority.
 */
- (NSLayoutConstraint *)shaka_equalConstraintForAttribute:(NSLayoutAttribute)attribute
                                                 fromItem:(UIView *)fromItem
                                                   toItem:(UIView *)toItem
                                             withPriority:(UILayoutPriority)priority;

/**
 * Link two views together, making them have a given attribute have a given relation.
 * Also applies a custom priority.
 */
- (NSLayoutConstraint *)shaka_relationalConstraintForAttribute:(NSLayoutAttribute)attribute
                                                 fromItem:(UIView *)fromItem
                                                   toItem:(UIView *)toItem
                                                  withRelation:(NSLayoutRelation)relation
                                             withPriority:(UILayoutPriority)priority;

@end
