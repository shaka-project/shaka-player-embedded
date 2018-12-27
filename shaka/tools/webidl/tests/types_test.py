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


class TypesTest(test_common.TestBase):

  def _parse_type(self, code):
    """Returns the parsed type."""
    results = self.parser.parse('', 'dictionary F {%s foo;};' % code)
    self.assertEqual(1, len(results.types))
    self.assertEqual(1, len(results.types[0].members))
    self.assertEqual('foo', results.types[0].members[0].name)
    return results.types[0].members[0].type

  def test_simple_types(self):
    types = [
        'foo',
        'any',
        'DOMString',
        'long',
        'long long',
        'unsigned long',
        'unsigned long long',
        'short',
        'double',
        'unrestricted double',
    ]
    for name in types:
      t = self._parse_type(name)
      self.assertEqual(name, t.name)
      self.assertIs(t.nullable, False)
      self.assertIs(t.element_type, None)

  def test_nullable(self):
    t = self._parse_type('foo?')
    self.assertEqual('foo', t.name)
    self.assertIs(t.nullable, True)
    self.assertIs(t.element_type, None)

  def test_sequence(self):
    t = self._parse_type('sequence<Foo>')
    self.assertEqual('sequence', t.name)
    self.assertIs(t.nullable, False)
    self.assertTrue(t.element_type)
    self.assertEqual('Foo', t.element_type.name)
    self.assertIs(t.element_type.nullable, False)
    self.assertIs(t.element_type.element_type, None)

  def test_sequence_nullable(self):
    t = self._parse_type('sequence<Foo>?')
    self.assertIs(t.nullable, True)
    self.assertIs(t.element_type.nullable, False)

  def test_sequence_nullable_inner(self):
    t = self._parse_type('sequence<Foo?>')
    self.assertIs(t.nullable, False)
    self.assertIs(t.element_type.nullable, True)

  def test_frozen_array(self):
    t = self._parse_type('FrozenArray<Foo>')
    self.assertEqual('FrozenArray', t.name)
    self.assertTrue(t.element_type)
    self.assertEqual('Foo', t.element_type.name)

  def test_promise(self):
    t = self._parse_type('Promise<Foo>')
    self.assertEqual('Promise', t.name)
    self.assertTrue(t.element_type)
    self.assertEqual('Foo', t.element_type.name)

  def test_promise_void(self):
    t = self._parse_type('Promise<void>')
    self.assertEqual('Promise', t.name)
    self.assertTrue(t.element_type)
    self.assertEqual('void', t.element_type.name)

  def test_syntax_error(self):
    bad_code = [
        '',
        'void',
        'unsigned',
        'unsigned float',
        'unsigned unsigned long',
        'long unsigned',
        'unrestricted long',
        'unrestricted unsigned float',
        'float unrestricted',
        'double??',
        '?double',
        'Promise<double>?',
        'Promise',
        'Promise<double',
        'Promise<>',
        'Promise<?>',
        'sequence<void>',
        'FrozenArray<void>',
        'MyType<float>',
    ]
    for code in bad_code:
      with self.assertRaises(parser.IdlSyntaxError):
        self._parse_type(code)


if __name__ == '__main__':
  unittest.main()
