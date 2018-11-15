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

#import "AssetSelectionTableViewController.h"
#import "PlayerViewController.h"
#import "ConfigTableViewController.h"

@interface AssetSelectionTableViewController ()

@property NSArray<NSDictionary *> *assets;
@property NSDictionary *chosenAsset;

@end

@implementation AssetSelectionTableViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Apply the title to the tabBarController, rather than this controller itself.
  // This way, the title will be carried to all of the elements within the tabBarController, and
  // won't be used as the title for the tabBarItems.
  NSString *version = [PlayerViewController getShakaPlayerVersion];
  self.tabBarController.title = [NSString stringWithFormat:@"Shaka Player Demo %@", version];

  // Load config values, even if the user doesn't ever navigate to the config table.
  [ConfigTableViewController setup];

  NSString *path = [[NSBundle mainBundle] pathForResource:@"assets" ofType:@"plist"];
  self.assets = [NSArray arrayWithContentsOfFile:path];
  [self.tableView reloadData];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
  return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
  return self.assets.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  UITableViewCell *cell =
      [tableView dequeueReusableCellWithIdentifier:@"cell" forIndexPath:indexPath];

  NSDictionary *asset = self.assets[indexPath.row];
  [cell.textLabel setText:asset[@"title"]];
  cell.accessibilityTraits = UIAccessibilityTraitButton;

  return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  self.chosenAsset = self.assets[indexPath.row];
  [self performSegueWithIdentifier:@"pickAsset" sender:self];
}

#pragma mark - Navigation

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
  if ([segue.destinationViewController isKindOfClass:[PlayerViewController class]]) {
    PlayerViewController *vc = segue.destinationViewController;
    vc.assetURI = self.chosenAsset[@"uri"];
    NSMutableDictionary *configuration = [[NSMutableDictionary alloc] init];
    [configuration addEntriesFromDictionary:self.chosenAsset[@"configuration"]];
    [configuration addEntriesFromDictionary:[ConfigTableViewController getConfigurations]];
    vc.configuration = configuration;
  }
}

@end
