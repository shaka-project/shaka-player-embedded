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

#import "LicenseTableViewController.h"
#import "LicenseViewController.h"
#import "PlayerViewController.h"
#import <ShakaPlayerEmbedded/ShakaPlayerEmbedded.h>

@interface LicenseTableViewController()

@property NSMutableArray<NSString *> *licenseBeneficiaries;
@property NSMutableArray<NSString *> *licenses;

@end

@implementation LicenseTableViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  NSBundle *bundle = [NSBundle bundleForClass:[ShakaPlayer class]];
  NSString *path = [bundle pathForResource:@"licenses" ofType:@"json"];
  NSData *data = [NSData dataWithContentsOfFile:path];
  NSError *error;
  NSArray *contents = [NSJSONSerialization JSONObjectWithData:data
                                                      options:kNilOptions
                                                        error:&error];
  self.licenses = [[NSMutableArray alloc] init];
  self.licenseBeneficiaries = [[NSMutableArray alloc] init];
  if (error) {
    NSLog(@"Error loading licenses: %@", error);
  } else {
    for (NSDictionary *entry in contents) {
      [self.licenseBeneficiaries addObject:entry[@"name"]];
      [self.self.licenses addObject:entry[@"text"]];
    }
  }

  [self.tableView reloadData];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
  return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
  return self.licenses.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"cell"
                                                          forIndexPath:indexPath];
  cell.textLabel.text = self.licenseBeneficiaries[indexPath.row];
  cell.accessibilityTraits = UIAccessibilityTraitButton;

  return cell;
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  NSString *name = self.licenseBeneficiaries[indexPath.row];
  NSString *license = self.licenses[indexPath.row];
  NSDictionary *info = @{
    @"name": name,
    @"license": license
  };
  [self performSegueWithIdentifier:@"pickLicense" sender:info];
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
  if ([segue.destinationViewController isKindOfClass:[LicenseViewController class]]) {
    LicenseViewController *lc = segue.destinationViewController;
    NSDictionary *info = sender;
    lc.title = info[@"name"];
    lc.license = info[@"license"];
  }
}

@end
