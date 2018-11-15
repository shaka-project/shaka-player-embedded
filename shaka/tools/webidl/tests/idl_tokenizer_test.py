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

"""Defines a tokenizer for IDL files."""

import math
import unittest

from webidl import idl_tokenizer


class StringReaderTest(unittest.TestCase):

  def test_read(self):
    reader = idl_tokenizer.StringReader('foobar')
    self.assertEqual(reader.read(1), 'f')
    self.assertEqual(reader.read(2), 'oo')
    self.assertEqual(reader.read(1), 'b')
    self.assertEqual(reader.read(0), '')
    self.assertEqual(reader.read(6), 'ar')
    self.assertTrue(reader.eof())
    self.assertEqual(reader.read(6), '')

  def test_peek_until(self):
    reader = idl_tokenizer.StringReader('foo;bar')
    self.assertEqual(reader.peek_until(';'), 'foo;')
    reader.read(3)
    self.assertEqual(reader.peek_until(';'), ';')
    self.assertEqual(reader.peek_until(';', skip=1), ';bar')
    reader.read(1)
    self.assertEqual(reader.peek_until(';'), 'bar')
    reader.read(4)
    self.assertTrue(reader.eof())
    self.assertEqual(reader.peek_until(';'), '')

  def test_peek(self):
    reader = idl_tokenizer.StringReader('foobar')
    self.assertEqual(reader.peek(0), '')
    self.assertEqual(reader.peek(1), 'f')
    self.assertEqual(reader.peek(2), 'fo')
    self.assertEqual(reader.peek(20), 'foobar')

    self.assertEqual(reader.read(3), 'foo')
    self.assertEqual(reader.peek(1), 'b')
    self.assertEqual(reader.peek(3), 'bar')
    self.assertEqual(reader.peek(20), 'bar')

  def test_get_line(self):
    reader = idl_tokenizer.StringReader('foo\nbar\nbaz')
    self.assertEqual(reader.get_line(), 'foo')
    self.assertEqual(reader.read(2), 'fo')
    self.assertEqual(reader.get_line(), 'foo')
    self.assertEqual(reader.read(1), 'o')
    self.assertEqual(reader.get_line(), 'foo')
    self.assertEqual(reader.read(1), '\n')
    self.assertEqual(reader.get_line(), 'bar')
    self.assertEqual(reader.read(3), 'bar')
    self.assertEqual(reader.get_line(), 'bar')
    self.assertEqual(reader.read(2), '\nb')
    self.assertEqual(reader.get_line(), 'baz')
    self.assertEqual(reader.read(2), 'az')
    self.assertTrue(reader.eof())
    self.assertEqual(reader.get_line(), 'baz')

    reader = idl_tokenizer.StringReader('foo')
    self.assertEqual(reader.get_line(), 'foo')
    self.assertEqual(reader.read(2), 'fo')
    self.assertEqual(reader.get_line(), 'foo')
    self.assertEqual(reader.read(1), 'o')
    self.assertEqual(reader.get_line(), 'foo')

  def test_tracks_line_and_col(self):
    reader = idl_tokenizer.StringReader('foo\nbar\ni = 1;\nj = 2\nk')
    self._assert_line_col(reader, 1, 1)
    self.assertEqual(reader.read(2), 'fo')
    self._assert_line_col(reader, 1, 3)
    self.assertEqual(reader.read(2), 'o\n')
    self._assert_line_col(reader, 2, 1)
    self.assertEqual(reader.read(7), 'bar\ni =')
    self._assert_line_col(reader, 3, 4)
    self.assertEqual(reader.read(10), ' 1;\nj = 2\n')
    self._assert_line_col(reader, 5, 1)

  def _assert_line_col(self, reader, line, col):
    self.assertEqual(reader.line, line)
    self.assertEqual(reader.col, col)


class IdlTokenizerTest(unittest.TestCase):

  def test_reads_special_chars(self):
    reader = idl_tokenizer.IdlTokenizer('file', '{}(),;?<>')
    self.assertEqual(reader.next().type,
                     idl_tokenizer.IdlTokenType.BEGIN_INTERFACE)
    self.assertEqual(reader.next().type,
                     idl_tokenizer.IdlTokenType.END_INTERFACE)
    self.assertEqual(reader.next().type, idl_tokenizer.IdlTokenType.BEGIN_ARGS)
    self.assertEqual(reader.next().type, idl_tokenizer.IdlTokenType.END_ARGS)
    self.assertEqual(reader.next().type, idl_tokenizer.IdlTokenType.COMMA)
    self.assertEqual(reader.next().type, idl_tokenizer.IdlTokenType.SEMI_COLON)
    self.assertEqual(reader.next().type, idl_tokenizer.IdlTokenType.NULLABLE)
    self.assertEqual(reader.next().type,
                     idl_tokenizer.IdlTokenType.BEGIN_TEMPLATE)
    self.assertEqual(reader.next().type,
                     idl_tokenizer.IdlTokenType.END_TEMPLATE)
    self.assertIs(reader.next(), None)

  def test_reads_numbers(self):
    reader = idl_tokenizer.IdlTokenizer(
        'file',
        '1234 0x777 0XabcDEF 3e5;7e-3 -999 -0x345 .123 0 0123 NaN Infinity '
        '-Infinity')
    def check_token(value):
      token = reader.next()
      self.assertEqual(token.type, idl_tokenizer.IdlTokenType.NUMBER_LITERAL)
      if math.isnan(value):
        self.assertTrue(math.isnan(token.value))
      else:
        self.assertEqual(token.value, value)

    check_token(1234)
    check_token(0x777)
    check_token(0xabcdef)
    check_token(3e5)
    self.assertEqual(reader.next().type, idl_tokenizer.IdlTokenType.SEMI_COLON)
    check_token(7e-3)
    check_token(-999)
    check_token(-0x345)
    check_token(.123)
    check_token(0)
    check_token(0o123)
    check_token(float('nan'))
    check_token(float('inf'))
    check_token(float('-inf'))

    bad_code = ['--2', '3-3', '5abc', '093', '3ee34', '3.4.2', '3x52', '24yz']
    for s in bad_code:
      reader = idl_tokenizer.IdlTokenizer('file', s)
      with self.assertRaises(SyntaxError):
        reader.next()

  def test_reads_strings(self):
    reader = idl_tokenizer.IdlTokenizer('file', '"foobar" "baz" ""')
    def check_token(value):
      token = reader.next()
      self.assertEqual(token.type, idl_tokenizer.IdlTokenType.STRING_LITERAL)
      self.assertEqual(token.value, value)

    check_token('foobar')
    check_token('baz')
    check_token('')

    bad_code = ['"foo', '"foo\nbar"']
    for s in bad_code:
      reader = idl_tokenizer.IdlTokenizer('file', s)
      with self.assertRaises(SyntaxError):
        reader.next()

  def test_ignores_escapes_in_strings(self):
    reader = idl_tokenizer.IdlTokenizer('file', '"foo\\nbar"')
    token = reader.next()
    self.assertEqual(token.type, idl_tokenizer.IdlTokenType.STRING_LITERAL)
    self.assertEqual(token.value, 'foo\\nbar')

  def test_allows_number_prefixes_in_identifier(self):
    # This ensures the tokenizer doesn't just look at the next few characters.
    reader = idl_tokenizer.IdlTokenizer(
        '', 'InfinityFoo Infinity_Bar Infinity;NaNFoo')
    self.assertEqual(reader.next().type, idl_tokenizer.IdlTokenType.IDENTIFIER)
    self.assertEqual(reader.next().type, idl_tokenizer.IdlTokenType.IDENTIFIER)
    self.assertEqual(
        reader.next().type, idl_tokenizer.IdlTokenType.NUMBER_LITERAL)
    self.assertEqual(reader.next().type, idl_tokenizer.IdlTokenType.SEMI_COLON)
    self.assertEqual(reader.next().type, idl_tokenizer.IdlTokenType.IDENTIFIER)

  def test_reads_identifiers(self):
    reader = idl_tokenizer.IdlTokenizer(
        'file', 'dictionary foo bar2s;foo foo-bar')
    self.assertEqual(reader.next().type, idl_tokenizer.IdlTokenType.DICTIONARY)

    token = reader.next()
    self.assertEqual(token.type, idl_tokenizer.IdlTokenType.IDENTIFIER)
    self.assertEqual(token.value, 'foo')

    token = reader.next()
    self.assertEqual(token.type, idl_tokenizer.IdlTokenType.IDENTIFIER)
    self.assertEqual(token.value, 'bar2s')

    self.assertEqual(reader.next().type, idl_tokenizer.IdlTokenType.SEMI_COLON)

    token = reader.next()
    self.assertEqual(token.type, idl_tokenizer.IdlTokenType.IDENTIFIER)
    self.assertEqual(token.value, 'foo')

    token = reader.next()
    self.assertEqual(token.type, idl_tokenizer.IdlTokenType.IDENTIFIER)
    self.assertEqual(token.value, 'foo-bar')

  def test_reads_comments(self):
    reader = idl_tokenizer.IdlTokenizer('file', '/** Comments */ foo')
    token = reader.next()
    self.assertEqual(token.type, idl_tokenizer.IdlTokenType.IDENTIFIER)
    self.assertEqual(token.value, 'foo')
    self.assertEqual(token.doc, '/** Comments */')

    # Only handles jsdoc comments: /** */
    reader = idl_tokenizer.IdlTokenizer('file', '/* Comments */ foo')
    self.assertIs(reader.next().doc, None)
    reader = idl_tokenizer.IdlTokenizer('file', '// Comments\nfoo')
    self.assertIs(reader.next().doc, None)

    reader = idl_tokenizer.IdlTokenizer('file', '/**/ foo')
    self.assertIs(reader.peek().doc, None)
    self.assertEqual(reader.peek().value, 'foo')
    reader = idl_tokenizer.IdlTokenizer('file', '/*/ foo */ end')
    self.assertEqual(reader.next().value, 'end')

    # Preserves leading whitespace.
    reader = idl_tokenizer.IdlTokenizer(
        'file', '/* first */\n  /**\n   * foo\n   */ bar')
    self.assertEqual(reader.next().doc, '  /**\n   * foo\n   */')

  def test_raises_syntax_error(self):
    bad_code = [
        '/* foobar ', '/*/', '/& foobar ', '/; foobar ', '$', '@', '!', '%', '^'
    ]
    for code in bad_code:
      reader = idl_tokenizer.IdlTokenizer('file', code)
      with self.assertRaises(SyntaxError):
        reader.next()


if __name__ == '__main__':
  unittest.main()
