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

import math
import unittest

from . import test_common
from webidl import parser
from webidl import types


class DefaultsTest(test_common.TestBase):

  def _parse_default(self, code):
    """Returns the parsed default."""
    results = self.parser.parse('', 'dictionary F {long x = %s;};' % code)
    self.assertEqual(1, len(results.types))
    self.assertEqual(1, len(results.types[0].members))
    self.assertEqual('x', results.types[0].members[0].name)
    return results.types[0].members[0].default

  def test_numbers(self):
    self.assertEqual(self._parse_default('0'), 0)
    self.assertEqual(self._parse_default('1234'), 1234)
    self.assertEqual(self._parse_default('1.2345e12'), 1.2345e12)
    self.assertEqual(self._parse_default('-999'), -999)
    self.assertEqual(self._parse_default('-.901'), -.901)
    self.assertEqual(self._parse_default('Infinity'), float('inf'))
    self.assertEqual(self._parse_default('-Infinity'), float('-inf'))
    self.assertTrue(math.isnan(self._parse_default('NaN')))

  def test_boolean(self):
    self.assertEqual(self._parse_default('true'), True)
    self.assertEqual(self._parse_default('false'), False)

  def test_null(self):
    self.assertIs(self._parse_default('null'), types.IdlNull)

  def test_string(self):
    self.assertEqual(self._parse_default('"foo"'), 'foo')
    self.assertEqual(self._parse_default('""'), '')
    self.assertEqual(self._parse_default('"foo\nbar"'), 'foo\nbar')

  def test_array(self):
    self.assertEqual(self._parse_default('[]'), [])

  def test_syntax_error(self):
    bad_code = [
        '',
        '1.2.3',
        '+1234',
        'x123',
        '0b123',
        '0x3g2',
        '--2',
        '-',
        'foobar',
    ]
    for code in bad_code:
      with self.assertRaises(parser.IdlSyntaxError):
        self._parse_default(code)

if __name__ == '__main__':
  unittest.main()
