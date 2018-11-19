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

import unittest

from . import test_common
from webidl import parser


class ArgumentsTest(test_common.TestBase):

  def _parse_arguments(self, code):
    """Parses an argument list into a list of Argument objects."""
    results = self.parser.parse('', '[ArgList(%s)] dictionary Foo {};' % code)
    self.assertEqual(len(results.types), 1)
    self.assertEqual(results.types[0].name, 'Foo')
    self.assertEqual(len(results.types[0].extensions), 1)
    self.assertIsInstance(results.types[0].extensions[0],
                          test_common.ArgListExtension)
    return results.types[0].extensions[0].args

  def _assert_argument_fields(self, arg, name, type_name, is_optional=False,
                              is_variadic=False, default=None):
    """Asserts the given argument has the required values."""
    self.assertEqual(arg.name, name)
    self.assertEqual(arg.type.name, type_name)
    self.assertEqual(arg.optional, is_optional)
    self.assertEqual(arg.is_variadic, is_variadic)
    self.assertEqual(arg.default, default)

  def test_one_argument(self):
    args = self._parse_arguments('long foo')
    self.assertEqual(len(args), 1)
    self._assert_argument_fields(args[0], name='foo', type_name='long')

  def test_three_arguments(self):
    args = self._parse_arguments('long foo, long bar, DOMString baz')
    self.assertEqual(len(args), 3)
    self._assert_argument_fields(args[0], name='foo', type_name='long')
    self._assert_argument_fields(args[1], name='bar', type_name='long')
    self._assert_argument_fields(args[2], name='baz', type_name='DOMString')

  def test_long_number_type_argument(self):
    args = self._parse_arguments('unsigned long long foo')
    self.assertEqual(len(args), 1)
    self._assert_argument_fields(args[0], name='foo',
                                 type_name='unsigned long long')

  def test_optional_argument(self):
    args = self._parse_arguments('optional long foo')
    self.assertEqual(len(args), 1)
    self._assert_argument_fields(args[0], name='foo', type_name='long',
                                 is_optional=True)

  def test_optional_with_default_argument(self):
    args = self._parse_arguments('optional long foo = 123')
    self.assertEqual(len(args), 1)
    self._assert_argument_fields(args[0], name='foo', type_name='long',
                                 is_optional=True, default=123)

  def test_keyword_argument(self):
    keywords = ['enum', 'static']
    for keyword in keywords:
      args = self._parse_arguments('long ' + keyword)
      self.assertEqual(len(args), 1)
      self._assert_argument_fields(args[0], name=keyword, type_name='long')

  def test_variadic_argument(self):
    args = self._parse_arguments('long... foo')
    self.assertEqual(len(args), 1)
    self._assert_argument_fields(args[0], name='foo', type_name='long',
                                 is_variadic=True)

  def test_arguments_syntax_error(self):
    bad_code = [
        'long',
        'long foo,',
        'void foo',
        'long foo = 1',
        'long foo bar',
        'optional long',
        'long optional foo',
        'optional foo',
        'optional long foo =',
        'optional long foo = bar',
        'optional long... args',
        'optional long x, long y',
        'long x, optional long y, long z',
        'long... args, long y',
        'long x, long... args, long y',
    ]
    for code in bad_code:
      with self.assertRaises(parser.IdlSyntaxError):
        self._parse_arguments(code)


if __name__ == '__main__':
  unittest.main()
