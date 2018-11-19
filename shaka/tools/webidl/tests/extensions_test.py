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
from webidl import types


class ExtensionsTest(test_common.TestBase):

  def _parse_definition_extensions(self, code):
    """Parses a extension that is attached to a definition."""
    results = self.parser.parse('', code + ' dictionary Foo {};')
    self.assertEqual(len(results.types), 1)
    self.assertEqual(results.types[0].name, 'Foo')
    return results.types[0].extensions

  def test_no_args_extension(self):
    extensions = self._parse_definition_extensions('[NoArgs]')
    self.assertEqual(len(extensions), 1)
    self.assertIsInstance(extensions[0], test_common.NoArgsExtension)

  def test_ident_extension(self):
    extensions = self._parse_definition_extensions('[Ident=Foo]')
    self.assertEqual(len(extensions), 1)
    self.assertIsInstance(extensions[0], test_common.IdentExtension)
    self.assertEqual(extensions[0].arg, 'Foo')

  def test_ident_list_extension(self):
    extensions = self._parse_definition_extensions('[IdentList=(Foo, Bar)]')
    self.assertEqual(len(extensions), 1)
    self.assertIsInstance(extensions[0], test_common.IdentListExtension)
    self.assertEqual(extensions[0].args, ['Foo', 'Bar'])

  def test_arg_list_extension(self):
    extensions = self._parse_definition_extensions('[ArgList(long foo)]')
    self.assertEqual(len(extensions), 1)
    self.assertIsInstance(extensions[0], test_common.ArgListExtension)
    self.assertEqual(len(extensions[0].args), 1)
    self.assertEqual(extensions[0].args[0].name, 'foo')
    self.assertEqual(extensions[0].args[0].type.name, 'long')

  def test_named_arg_list_extension(self):
    extensions = self._parse_definition_extensions(
        '[NamedArgList=Name(long foo)]')
    self.assertEqual(len(extensions), 1)
    self.assertIsInstance(extensions[0], test_common.NamedArgListExtension)
    self.assertEqual(extensions[0].argName, 'Name')
    self.assertEqual(len(extensions[0].args), 1)
    self.assertEqual(extensions[0].args[0].name, 'foo')
    self.assertEqual(extensions[0].args[0].type.name, 'long')

  def test_multiple_kinds(self):
    # Define a new extension that can be used either with no arguments or with
    # a single identifier to test that both can be used and that other usages
    # are still invalid.
    class MyExtension(types.Extension):

      name = 'My'
      kinds = [types.ExtensionKind.NO_ARGS, types.ExtensionKind.IDENT]
      # Allow this everywhere.
      locations = list(types.ExtensionLocation)

      def __init__(self, arg=None):
        # Depending on how the extension appears, this will be called with no
        # arguments or with one named arg.
        self.arg = arg

    results = parser.ParseFile(
        '', '[My, My=Foo] dictionary Foo {};', extensions=[MyExtension])
    self.assertEqual(len(results.types), 1)
    self.assertEqual(len(results.types[0].extensions), 2)
    self.assertIsInstance(results.types[0].extensions[0], MyExtension)
    self.assertIsInstance(results.types[0].extensions[1], MyExtension)
    self.assertIs(results.types[0].extensions[0].arg, None)
    self.assertEqual(results.types[0].extensions[1].arg, 'Foo')

    with self.assertRaises(parser.IdlSyntaxError):
      parser.ParseFile(
          '', '[My=(Foo, Bar)] dictionary Foo {};', extensions=[MyExtension])

  def test_multiple_extensions(self):
    extensions = self._parse_definition_extensions(
        '[NoArgs, NoArgs, Ident=Foo]')
    self.assertEqual(len(extensions), 3)
    self.assertIsInstance(extensions[0], test_common.NoArgsExtension)
    self.assertIsInstance(extensions[1], test_common.NoArgsExtension)
    self.assertIsInstance(extensions[2], test_common.IdentExtension)
    self.assertEqual(extensions[2].arg, 'Foo')

  def test_extensions_reports_multiple_errors(self):
    try:
      self._parse_definition_extensions('[NoArgs=Foo, Bar]')
      self.fail('Should throw syntax error')
    except parser.IdlSyntaxError as e:
      self.assertEqual(2, len(e.inner_errors))
      self.assertEqual(1, e.inner_errors[0].lineno)
      self.assertEqual(2, e.inner_errors[0].offset)
      self.assertEqual(1, e.inner_errors[1].lineno)
      self.assertEqual(14, e.inner_errors[1].offset)

  def test_extensions_reports_extension_exceptions(self):
    class MyExt(types.Extension):
      name = 'My'
      kinds = [types.ExtensionKind.NO_ARGS]
      locations = list(types.ExtensionLocation)

      def __init__(self):
        raise RuntimeError('My error')

    try:
      parser.ParseFile('', '[My] dictionary Foo {};', extensions=[MyExt])
      self.fail('Should throw syntax error')
    except parser.IdlSyntaxError as e:
      self.assertEqual(1, len(e.inner_errors))
      self.assertEqual(1, e.inner_errors[0].lineno)
      self.assertEqual(2, e.inner_errors[0].offset)
      self.assertIsInstance(e.inner_errors[0], parser.ExtensionError)
      self.assertIsInstance(e.inner_errors[0].inner, RuntimeError)
      self.assertEqual('My error', str(e.inner_errors[0].inner))

  def test_extensions_syntax_error(self):
    bad_code = [
        '[]',
        '[UnknownExtension]',
        '[IdentList=',
        '[IdentList=]',
        '[IdentList=optional]',
        '[IdentList=()]',
        '[IdentList=(Foo]',
        '[IdentList=(Foo,]',
        '[IdentList=(Foo,,)]',
        '[IdentList=(Foo Bar)]',
        '[NoArgs,]',
        '[NoArgs,,]',
        '[NoArgs,, NoArgs]',
        '[NoArgs NoArgs]',
        '[ArgList(]',

        '[NoArgs=foo]',
        '[ArgList=Foo()]',
        '[Ident=(Foo, Bar)]',
        '[IdentList(int i)]',
        '[NamedArgList]',
    ]
    for code in bad_code:
      with self.assertRaises(parser.IdlSyntaxError):
        self._parse_definition_extensions(code)


if __name__ == '__main__':
  unittest.main()
