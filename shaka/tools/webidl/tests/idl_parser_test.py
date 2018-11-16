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

from webidl import idl_parser
from webidl import idl_tokenizer
from webidl import types


class NoArgsExtension(types.Extension):
  name = 'NoArgs'
  kind = types.ExtensionKind.NO_ARGS
  # Allow this everywhere.
  locations = list(types.ExtensionLocation)


class ArgListExtension(types.Extension):
  name = 'ArgList'
  kind = types.ExtensionKind.ARG_LIST
  # Allow this everywhere.
  locations = list(types.ExtensionLocation)


class NamedArgListExtension(types.Extension):
  name = 'NamedArgList'
  kind = types.ExtensionKind.NAMED_ARG_LIST
  # Allow this everywhere.
  locations = list(types.ExtensionLocation)


class IdentExtension(types.Extension):
  name = 'Ident'
  kind = types.ExtensionKind.IDENT
  # Allow this everywhere.
  locations = list(types.ExtensionLocation)


class IdentListExtension(types.Extension):
  name = 'IdentList'
  kind = types.ExtensionKind.IDENT_LIST
  # Allow this everywhere.
  locations = list(types.ExtensionLocation)


ALL_EXTENSIONS = [
    NoArgsExtension, ArgListExtension, NamedArgListExtension, IdentExtension,
    IdentListExtension,
]


class ParserTest(unittest.TestCase):

  def assertExtensionValue(self, actual, type_, **kwargs):
    self.assertIsInstance(actual, type_)
    self.assertEqual(vars(actual), kwargs)

  def test_empty_file(self):
    results = idl_parser.IdlParser().parse_file('file.idl', '')
    self.assertEqual(results.types, [])

    results = idl_parser.IdlParser().parse_file(
        'file.idl', '// Copyright 2018 Google Inc.\n  \n/** Foobar */')
    self.assertEqual(results.types, [])

  def test_empty_dictionary(self):
    results = idl_parser.IdlParser().parse_file(
        'file.idl', '/** Doc */ dictionary Foo {};')
    self.assertEqual(len(results.types), 1)
    self.assertIsInstance(results.types[0], types.Dictionary)
    self.assertEqual(results.types[0].name, 'Foo')
    self.assertEqual(results.types[0].attributes, [])
    self.assertEqual(results.types[0].doc, '/** Doc */')

  def test_parses_types(self):
    results = idl_parser.IdlParser().parse_file(
        'file.idl',
        """dictionary Foo {
          /** Foobar */
          double attr1;
          // Foobar
          double? attr2;
          sequence<double> attr3;
          sequence<double?>? attr4;
        };""")
    self.assertEqual(len(results.types), 1)

    attrs = results.types[0].attributes
    self.assertEqual(len(attrs), 4)
    expected_attrs = [
        types.Attribute(
            name='attr1', debug=None, docDebug=None,
            doc='          /** Foobar */',
            type=types.IdlType(
                name='double', nullable=False, element_type=None)),
        types.Attribute(
            name='attr2', debug=None, docDebug=None,
            doc=None,
            type=types.IdlType(
                name='double', nullable=True, element_type=None)),
        types.Attribute(
            name='attr3', debug=None, docDebug=None,
            doc=None,
            type=types.IdlType(
                name='sequence', nullable=False,
                element_type=types.IdlType(name='double', nullable=False,
                                           element_type=None))),
        types.Attribute(
            name='attr4', debug=None, docDebug=None,
            doc=None,
            type=types.IdlType(
                name='sequence', nullable=True,
                element_type=types.IdlType(name='double', nullable=True,
                                           element_type=None))),
    ]
    self.assertEqual(attrs, expected_attrs)

  def test_parses_arg_lists(self):
    def _do_parse(body):
      parser = idl_parser.IdlParser()
      parser._reader = idl_tokenizer.IdlTokenizer('', body)
      return parser.read_arg_list()

    args = _do_parse('()')
    self.assertEqual(args, [])

    int_ = types.IdlType(name='int', nullable=False, element_type=None)

    args = _do_parse('(int foo, optional int bar)')
    self.assertEqual(args, [
        types.Argument(
            name='foo', optional=False, default=None, type=int_,
            is_variadic=False),
        types.Argument(
            name='bar', optional=True, default=None, type=int_,
            is_variadic=False),
    ])

    args = _do_parse('(int... foo)')
    self.assertEqual(args, [
        types.Argument(
            name='foo', optional=False, default=None, type=int_,
            is_variadic=True),
    ])

    args = _do_parse('(optional int foo = 123)')
    self.assertEqual(args, [
        types.Argument(
            name='foo', optional=True, default=123, type=int_,
            is_variadic=False),
    ])

  def test_arg_list_syntax_error(self):
    bad_code = [
        '(',
        '(int foo',
        '(int foo,)',
        '(optional foo)',
        '(int int foo)',
        '(int optional foo)',
        '(int... foo, int bar)',
        '(int foo = 1)',
        '(optional int foo = bar)',
        '(optional int foo =)',
        '(optional foo... bar)',
        '(optional int foo, int bar)',
    ]

    for code in bad_code:
      with self.assertRaises(SyntaxError):
        parser = idl_parser.IdlParser()
        parser._reader = idl_tokenizer.IdlTokenizer('', code)
        parser.read_arg_list()

  def test_extensions_no_args(self):
    parser = idl_parser.IdlParser(ALL_EXTENSIONS)
    parser._reader = idl_tokenizer.IdlTokenizer('', '[NoArgs]')
    results = parser.read_extensions(
        [types.ExtensionLocation.DEFINITION])
    self.assertEqual(len(results), 1)
    self.assertExtensionValue(results[0], NoArgsExtension)

  def test_extensions_arg_list(self):
    parser = idl_parser.IdlParser(ALL_EXTENSIONS)
    parser._reader = idl_tokenizer.IdlTokenizer(
        '', '[ArgList(int x, optional long y)]')
    results = parser.read_extensions(
        [types.ExtensionLocation.DEFINITION])
    self.assertEqual(len(results), 1)
    args = [
        types.Argument(
            name='x', optional=False, default=None, is_variadic=False,
            type=types.IdlType(
                name='int', nullable=False, element_type=None)),
        types.Argument(
            name='y', optional=True, default=None, is_variadic=False,
            type=types.IdlType(
                name='long', nullable=False, element_type=None)),
    ]
    self.assertExtensionValue(results[0], ArgListExtension, args=args)

  def test_extensions_named_arg_list(self):
    parser = idl_parser.IdlParser(ALL_EXTENSIONS)
    parser._reader = idl_tokenizer.IdlTokenizer('', '[NamedArgList=Foo()]')
    results = parser.read_extensions(
        [types.ExtensionLocation.DEFINITION])
    self.assertEqual(len(results), 1)
    self.assertExtensionValue(
        results[0], NamedArgListExtension, argsName='Foo', args=[])

  def test_extensions_ident(self):
    parser = idl_parser.IdlParser(ALL_EXTENSIONS)
    parser._reader = idl_tokenizer.IdlTokenizer('', '[Ident=Something]')
    results = parser.read_extensions(
        [types.ExtensionLocation.DEFINITION])
    self.assertEqual(len(results), 1)
    self.assertExtensionValue(results[0], IdentExtension, arg='Something')

  def test_extensions_ident_list(self):
    parser = idl_parser.IdlParser(ALL_EXTENSIONS)
    parser._reader = idl_tokenizer.IdlTokenizer(
        '', '[IdentList=(First, Second)]')
    results = parser.read_extensions(
        [types.ExtensionLocation.DEFINITION])
    self.assertEqual(len(results), 1)
    self.assertExtensionValue(
        results[0], IdentListExtension, args=['First', 'Second'])

    # An identifier list can be specified as a single identifier too.
    parser._reader = idl_tokenizer.IdlTokenizer('', '[IdentList=First]')
    results = parser.read_extensions(
        [types.ExtensionLocation.DEFINITION])
    self.assertEqual(len(results), 1)
    self.assertExtensionValue(results[0], IdentListExtension, args=['First'])

  def test_extensions_multiples(self):
    parser = idl_parser.IdlParser(ALL_EXTENSIONS)
    parser._reader = idl_tokenizer.IdlTokenizer(
        '', '[NoArgs, Ident=first, Ident=second]')
    results = parser.read_extensions(
        [types.ExtensionLocation.DEFINITION])
    self.assertEqual(len(results), 3)
    self.assertExtensionValue(results[0], NoArgsExtension)
    self.assertExtensionValue(results[1], IdentExtension, arg='first')
    self.assertExtensionValue(results[2], IdentExtension, arg='second')

  def test_extensions_checks_location(self):
    class LocationExtension(types.Extension):
      name = 'Foo'
      kind = types.ExtensionKind.NO_ARGS
      locations = [types.ExtensionLocation.MEMBER]

    parser = idl_parser.IdlParser([LocationExtension])
    with self.assertRaises(SyntaxError):
      parser._reader = idl_tokenizer.IdlTokenizer('', '[Foo]')
      parser.read_extensions([types.ExtensionLocation.DEFINITION])

    with self.assertRaises(SyntaxError):
      parser._reader = idl_tokenizer.IdlTokenizer('', '[Foo]')
      locs = [types.ExtensionLocation.MIXIN_MEMBER,
              types.ExtensionLocation.TYPE]
      parser.read_extensions(locs)

    # So long as one matches it should work.
    parser._reader = idl_tokenizer.IdlTokenizer('', '[Foo]')
    locs = [types.ExtensionLocation.MEMBER,
            types.ExtensionLocation.TYPE]
    results = parser.read_extensions(locs)
    self.assertEqual(len(results), 1)
    self.assertExtensionValue(results[0], LocationExtension)

  def test_extensions_syntax_error(self):
    bad_code = [
        '[]',
        '[IdentList=',
        '[IdentList=]',
        '[IdentList=optional]',
        '[IdentList=()]',
        '[IdentList=(Foo,]',
        '[IdentList=(Foo,,)]',
        '[IdentList=(Foo Bar)]',
        '[IdentList=(Foo]',
        '[NoArgs,]',
        '[NoArgs,,]',
        '[NoArgs NoArgs]',

        '[NoArgs=foo]',
        '[ArgList=Foo()]',
        '[Ident=(Foo, Bar)]',
        '[IdentList(int i)]',
        '[NamedArgList]',
    ]
    for code in bad_code:
      with self.assertRaises(SyntaxError):
        parser = idl_parser.IdlParser(ALL_EXTENSIONS)
        parser._reader = idl_tokenizer.IdlTokenizer('', code)
        parser.read_extensions([types.ExtensionLocation.DEFINITION])

  def test_syntax_error(self):
    bad_code = [
        'foo', 'dictionary', 'dictionary {', 'dictionary Foo',
        'dictionary Foo {', 'dictionary Foo {}', 'dictionary Foo { double };',
        'dictionary Foo { double a };',
        'dictionary Foo { double?? a; };',
        'dictionary Foo { ?double a; };',
        'dictionary Foo { Bar<double> foo; };',
        'dictionary Foo { double bar foo; };',
        'dictionary Foo { sequence<double foo; };',
        'dictionary Foo { sequence<double??> foo; };',
    ]
    for code in bad_code:
      with self.assertRaises(SyntaxError):
        idl_parser.IdlParser().parse_file('foo.idl', code)

  def test_cleans_docstrings(self):
    doc = """/** This is a really
        * long docstring.
        * This is still going.
        */"""
    code = doc + ' dictionary Foo {};'
    results = idl_parser.IdlParser().parse_file('foo.idl', code)
    self.assertEqual(results.types[0].doc, doc)


if __name__ == '__main__':
  unittest.main()
