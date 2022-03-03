#!/usr/bin/python
# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Unit tests for the version parsing code."""

from __future__ import print_function

import unittest

import version


class VersionTest(unittest.TestCase):

  def check_version(self, input_str, expected):
    actual = version.ParseVersion(input_str)
    self.assertEqual(actual, expected)

  def compare_versions_greater(self, greater, less):
    a = version.ParseVersion(greater)
    b = version.ParseVersion(less)
    # Compare using integers instead of tuples because this is how C++ works.
    self.assertGreater(a[0]<<48 | a[1]<<32 | a[2]<<16 | a[3],
                       b[0]<<48 | b[1]<<32 | b[2]<<16 | b[3],
                       'Version "%s" not greater than "%s"' % (greater, less))

  def test_basic_case(self):
    self.check_version('v1.2.3', (1, 2, 3, 0))

  def test_beta_versions(self):
    # Beta releases occur before the number that appears.
    self.check_version('v1.2.3-beta', (1, 2, 2, 1<<13))
    self.check_version('v1.2.3-beta4', (1, 2, 2, 4<<13))
    self.check_version('v1.2.0-beta', (1, 1, 2**16-1, 1<<13))
    self.check_version('v1.0.0-beta', (0, 2**16-1, 2**16-1, 1<<13))

  def test_with_commits(self):
    self.check_version('v1.0.0-123-gbc123456', (1, 0, 0, 123<<1))

  def test_dirty(self):
    self.check_version('v1.0.0-dirty', (1, 0, 0, 1))

  def test_all(self):
    self.check_version('v3.2.1-beta3-123-gbc123456-dirty',
                       (3, 2, 0, 3<<13 | 123<<1 | 1))

  def test_compare(self):
    self.compare_versions_greater('v1.2.3', 'v1.2.2')
    self.compare_versions_greater('v1.2.3', 'v1.2.0')
    self.compare_versions_greater('v1.2.3', 'v1.1.20')
    self.compare_versions_greater('v1.2.3', 'v0.9.10')
    self.compare_versions_greater('v1.2.3', 'v1.2.3-beta5')
    self.compare_versions_greater('v1.2.3-dirty', 'v1.2.2')
    self.compare_versions_greater('v1.2.3-dirty', 'v1.2.3')
    self.compare_versions_greater('v1.2.3-dirty', 'v1.2.3-beta')
    self.compare_versions_greater('v1.2.3-123-gbc123456', 'v1.2.2')
    self.compare_versions_greater('v1.2.3-123-gbc123456', 'v1.2.3')
    self.compare_versions_greater('v1.2.3-123-gbc123456', 'v1.2.3-beta4')
    self.compare_versions_greater('v1.2.3-123-gbc123456',
                                  'v1.2.3-121-ge922add2')
    self.compare_versions_greater('v1.2.3-123-gbc123456-dirty',
                                  'v1.2.3-123-gbc123456')
    self.compare_versions_greater('v1.2.3-beta2-1-gbc123456',
                                  'v1.2.3-beta-92-gbc123456')

  def test_errors(self):
    bad = [
        '',
        'main',
        'master',
        'v0.0.0',
        'v0.0.0-beta2',
        'v1.2',
        '1.2.3',
        'v2.3.4-alpha',
        'v2.3.4-beta500',  # Too big a beta version.
        'v2.3.4-10000-gbc123456',  # Too many commits since release.
        'v200000.3.4',  # Too big major version.
        'v2.300000.4',  # Too big minor version.
        'v2.3.400000',  # Too big revision version.
    ]
    for code in bad:
      with self.assertRaises(ValueError):
        version.ParseVersion(code)

if __name__ == '__main__':
  unittest.main()
