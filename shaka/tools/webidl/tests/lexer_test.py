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

"""Defines tests for the lexer."""

import math
import unittest

from webidl import lexer


class IdlLexerTest(unittest.TestCase):

  def __init__(self, *args, **kwargs):
    super(IdlLexerTest, self).__init__(*args, **kwargs)
    self.lexer = lexer.IdlLexer()

  def test_tracks_line_and_col(self):
    self.lexer.set_contents(
        'file', 'foo     foo\nfoo /* bar */ baz  \n    cat\n\n\n  foo')

    def check_token(value, line, col):
      token = self.lexer.token()
      self.assertEqual(token.type, 'IDENTIFIER')
      self.assertEqual(token.value, value)
      self.assertEqual(token.lineno, line)
      self.assertEqual(self.lexer.get_col(token.lexpos), col)

    check_token('foo', 1, 1)
    check_token('foo', 1, 9)
    check_token('foo', 2, 1)
    check_token('baz', 2, 15)
    check_token('cat', 3, 5)
    check_token('foo', 6, 3)

  def test_reads_special_chars(self):
    self.lexer.set_contents('file', '{}(),;?<>')
    self.assertEqual(self.lexer.token().type, '{')
    self.assertEqual(self.lexer.token().type, '}')
    self.assertEqual(self.lexer.token().type, '(')
    self.assertEqual(self.lexer.token().type, ')')
    self.assertEqual(self.lexer.token().type, ',')
    self.assertEqual(self.lexer.token().type, ';')
    self.assertEqual(self.lexer.token().type, '?')
    self.assertEqual(self.lexer.token().type, '<')
    self.assertEqual(self.lexer.token().type, '>')
    self.assertIs(self.lexer.token(), None)

  def test_reads_numbers(self):
    self.lexer.set_contents(
        'file', '1234 0x777 0XabcDEF 3e5;7e-3 -999 -0x345 .123 0 0123 -0123')
    def check_token(type_, value):
      token = self.lexer.token()
      self.assertEqual(token.type, type_)
      self.assertEqual(token.value, value)

    check_token('INTEGER_LITERAL', 1234)
    check_token('INTEGER_LITERAL', 0x777)
    check_token('INTEGER_LITERAL', 0xabcdef)
    check_token('FLOAT_LITERAL', 3e5)
    self.assertEqual(self.lexer.token().type, ';')
    check_token('FLOAT_LITERAL', 7e-3)
    check_token('INTEGER_LITERAL', -999)
    check_token('INTEGER_LITERAL', -0x345)
    check_token('FLOAT_LITERAL', .123)
    check_token('INTEGER_LITERAL', 0)
    check_token('INTEGER_LITERAL', 0o123)
    check_token('INTEGER_LITERAL', -0o123)

  def test_reads_strings(self):
    self.lexer.set_contents('file', '"foobar" "baz" ""')
    def check_token(value):
      token = self.lexer.token()
      self.assertEqual(token.type, 'STRING_LITERAL')
      self.assertEqual(token.value, value)

    check_token('foobar')
    check_token('baz')
    check_token('')

    bad_code = ['"foo']
    for s in bad_code:
      self.lexer.set_contents('file', s)
      with self.assertRaises(SyntaxError):
        self.lexer.token()

  def test_ignores_escapes_in_strings(self):
    self.lexer.set_contents('file', r'"foo\nbar"')
    token = self.lexer.token()
    self.assertEqual(token.type, 'STRING_LITERAL')
    self.assertEqual(token.value, r'foo\nbar')

    self.lexer.set_contents('file', r'"foo\"')
    token = self.lexer.token()
    self.assertEqual(token.type, 'STRING_LITERAL')
    self.assertEqual(token.value, 'foo\\')

  def test_reads_identifiers(self):
    self.lexer.set_contents(
        'file', 'dictionary foo bar2s;foo foo-bar')
    self.assertEqual(self.lexer.token().type, 'DICTIONARY')

    token = self.lexer.token()
    self.assertEqual(token.type, 'IDENTIFIER')
    self.assertEqual(token.value, 'foo')

    token = self.lexer.token()
    self.assertEqual(token.type, 'IDENTIFIER')
    self.assertEqual(token.value, 'bar2s')

    self.assertEqual(self.lexer.token().type, ';')

    token = self.lexer.token()
    self.assertEqual(token.type, 'IDENTIFIER')
    self.assertEqual(token.value, 'foo')

    token = self.lexer.token()
    self.assertEqual(token.type, 'IDENTIFIER')
    self.assertEqual(token.value, 'foo-bar')

  def test_reads_docstrings(self):
    self.lexer.set_contents('file', '/** Comments */ foo')
    token = self.lexer.token()
    self.assertEqual(token.type, 'DOCSTRING')
    self.assertEqual(token.value, '/** Comments */')

    token = self.lexer.token()
    self.assertEqual(token.type, 'IDENTIFIER')
    self.assertEqual(token.value, 'foo')

  def test_skips_comments(self):
    self.lexer.set_contents('file', '/* Comments */ foo')
    token = self.lexer.token()
    self.assertEqual(token.type, 'IDENTIFIER')
    self.assertEqual(token.value, 'foo')

    self.lexer.set_contents('file', '// Comments\nfoo')
    self.assertEqual(token.value, 'foo')

    self.lexer.set_contents('file', '/**/ foo')
    token = self.lexer.token()
    self.assertEqual(token.type, 'IDENTIFIER')
    self.assertEqual(token.value, 'foo')

    self.lexer.set_contents('file', '/*/ foo */ end')
    self.assertEqual(self.lexer.token().value, 'end')

    # TODO: Preserves leading whitespace.
    # self.lexer.set_contents(
    #     'file', '/* first */\n  /**\n   * foo\n   */ bar')
    # self.assertEqual(self.lexer.next().doc, '  /**\n   * foo\n   */')

  def test_raises_syntax_error(self):
    bad_code = [
        '/* foobar ', '/*/', '/& foobar ', '/; foobar ', '$', '@', '!', '%', '^'
    ]
    for code in bad_code:
      self.lexer.set_contents('file', code)
      with self.assertRaises(SyntaxError):
        self.lexer.token()


if __name__ == '__main__':
  unittest.main()
