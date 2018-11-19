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


class OptionsTest(unittest.TestCase):

  def test_has_feature(self):
    options = parser.Options()
    self.assertFalse(options.has_feature('dictionary-required'))
    self.assertFalse(options.has_feature(parser.Features.DICTIONARY_REQUIRED))
    self.assertFalse(options.has_feature(parser.Features.DICTIONARY_DEFAULT))

    options = parser.Options(parser.Features.DICTIONARY_REQUIRED)
    self.assertTrue(options.has_feature('dictionary-required'))
    self.assertTrue(options.has_feature(parser.Features.DICTIONARY_REQUIRED))
    self.assertFalse(options.has_feature(parser.Features.DICTIONARY_DEFAULT))

  def test_assumed_feature(self):
    tests = [
        ('dictionary-required', 'dictionary', 'dictionary-default'),
    ]
    for feature, expected, not_added in tests:
      options = parser.Options(feature)
      self.assertTrue(options.has_feature(feature))
      self.assertTrue(options.has_feature(expected))
      self.assertFalse(options.has_feature(not_added))

  def test_raises_for_bad_feature(self):
    with self.assertRaises(ValueError):
      parser.Options('foobar')

    options = parser.Options()
    with self.assertRaises(ValueError):
      options.has_feature('foobar')


class MainTest(test_common.TestBase):

  def test_empty_file(self):
    results = self.parser.parse('', '')
    self.assertEqual(results.types, [])

    results = self.parser.parse(
        'file.idl', '// Copyright 2018 Google Inc.\n  \n/* Foobar */')
    self.assertEqual(results.types, [])

  def test_can_reuse_parser(self):
    # Get a syntax error.
    with self.assertRaises(parser.IdlSyntaxError):
      self.parser.parse('', 'foo bar baz')

    # Can reuse the parser and get the correct results.
    results = self.parser.parse('', '/** Foobar */ dictionary foo {};')
    self.assertEqual(len(results.types), 1)

  def test_enforces_options(self):
    tests = [
        (['dictionary'], 'dictionary Foo { long x; };'),
        (['dictionary-required'], 'dictionary Foo { required long x; };'),
        (['dictionary-default'], 'dictionary Foo { long x = 123; };'),
        (['dictionary-inherit'], 'dictionary Foo : Bar {};'),
        (['dictionary-partial'], 'partial dictionary Foo {};'),
    ]
    parse = parser.IdlParser()
    for config, code in tests:
      # With all options it should work.
      parse.options = parser.Options.all()
      parse.parse('', code)
      # It should also work with just the setting given.
      parse.options = parser.Options(*config)
      parse.parse('', code)
      # Without setting given, should raise an error.
      with self.assertRaises(parser.IdlSyntaxError):
        parse.options = parser.Options()
        parse.parse('', code)

if __name__ == '__main__':
  unittest.main()
