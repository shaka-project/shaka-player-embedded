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


class DictionaryTest(test_common.TestBase):

  def test_empty_dictionary(self):
    results = self.parser.parse(
        '', '/** Foobar */ dictionary foo {};')
    self.assertEqual(len(results.types), 1)

    self.assertEqual(results.types[0].name, 'foo')
    self.assertEqual(results.types[0].kind, 'dictionary')
    self.assertEqual(results.types[0].base, None)
    self.assertIs(results.types[0].is_partial, False)
    self.assertEqual(results.types[0].doc, '/** Foobar */')
    self.assertEqual(len(results.types[0].members), 0)
    self.assertEqual(results.types[0].debug.lineno, 1)
    self.assertEqual(results.types[0].debug.col, 15)
    self.assertEqual(results.types[0].debug.line,
                     '/** Foobar */ dictionary foo {};')
    self.assertEqual(results.types[0].docDebug.lineno, 1)
    self.assertEqual(results.types[0].docDebug.col, 1)
    self.assertEqual(results.types[0].docDebug.line,
                     '/** Foobar */ dictionary foo {};')

  def test_inherit(self):
    results = self.parser.parse('', 'dictionary Foo : Bar {};')
    self.assertEqual(len(results.types), 1)
    self.assertEqual(results.types[0].name, 'Foo')
    self.assertEqual(results.types[0].base, 'Bar')
    self.assertIs(results.types[0].is_partial, False)
    self.assertEqual(len(results.types[0].members), 0)

  def test_partial(self):
    results = self.parser.parse('', 'partial dictionary Foo {};')
    self.assertEqual(len(results.types), 1)
    self.assertEqual(results.types[0].name, 'Foo')
    self.assertIs(results.types[0].base, None)
    self.assertIs(results.types[0].is_partial, True)
    self.assertEqual(len(results.types[0].members), 0)

  def test_members(self):
    code = """
      dictionary foo {
        required double foo;
        unsigned long bar;
        DOMString baz = "foobar";
      };"""
    results = self.parser.parse('', code)
    self.assertEqual(len(results.types), 1)
    self.assertEqual(results.types[0].name, 'foo')
    self.assertEqual(len(results.types[0].members), 3)

    attrs = results.types[0].members
    self.assertEqual(attrs[0].name, 'foo')
    self.assertEqual(attrs[0].type.name, 'double')
    self.assertIs(attrs[0].is_required, True)
    self.assertIs(attrs[0].default, None)
    self.assertEqual(attrs[1].name, 'bar')
    self.assertEqual(attrs[1].type.name, 'unsigned long')
    self.assertIs(attrs[1].is_required, False)
    self.assertIs(attrs[1].default, None)
    self.assertEqual(attrs[2].name, 'baz')
    self.assertEqual(attrs[2].type.name, 'DOMString')
    self.assertIs(attrs[2].is_required, False)
    self.assertEqual(attrs[2].default, 'foobar')

  def test_handles_multiple_dictionary_errors(self):
    try:
      self.parser.parse('', 'dictionary foo { x };\ndictionary bar {y};')
      self.fail('Should throw syntax error')
    except parser.IdlSyntaxError as e:
      self.assertEqual(2, len(e.inner_errors))
      self.assertEqual(1, e.inner_errors[0].lineno)
      self.assertEqual(20, e.inner_errors[0].offset)
      self.assertEqual(2, e.inner_errors[1].lineno)
      self.assertEqual(18, e.inner_errors[1].offset)

  def test_syntax_error(self):
    bad_code = [
        'foo',
        'dictionary',
        'dictionary Foo {',
        'dictionary Foo {}',
        'dictionary {};',
        'dictionary {;',
        'dictionary Foo : {};',
        'dictionary Foo Bar {};',
        'dictionary partial Foo {};',
        'partial dictionary {};',
        'partial dictionary Foo : Bar {};',
    ]

    for code in bad_code:
      with self.assertRaises(parser.IdlSyntaxError):
        self.parser.parse('', code)


if __name__ == '__main__':
  unittest.main()
