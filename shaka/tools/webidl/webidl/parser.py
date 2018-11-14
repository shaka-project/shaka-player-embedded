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

"""Defines a lex parser for IDL files.

This uses PLY for the basis:
    http://www.dabeaz.com/ply/
This parses WebIDL syntax:
    https://heycam.github.io/webidl/#idl-grammar
"""

# pylint: disable=g-doc-args
# pylint: disable=g-doc-return-or-yield
# pylint: disable=g-docstring-missing-newline
# pylint: disable=g-no-space-after-docstring-summary
# pylint: disable=g-short-docstring-punctuation
# pylint: disable=invalid-name
# pylint: disable=line-too-long

import functools

from ply import lex
from ply import yacc
from webidl import lexer
from webidl import types


class IdlSyntaxError(SyntaxError):
  """Defines a SyntaxError that may contain multiple errors."""

  def __init__(self, errors):
    assert errors, 'Must provide at least one error'
    super(IdlSyntaxError, self).__init__(*errors[0].args)
    self.inner_errors = errors


def _rule(func):
  """A decorator for defining rules."""
  assert func.__doc__
  assert func.__name__.startswith('p_')
  name = func.__name__[2:]
  if name.endswith('_error'):
    name = name[:-6]
  assert func.__doc__.startswith(name + ' :')

  @functools.wraps(func)
  def wrapper(self, p):
    p[0] = func(self, p)
  return wrapper


class IdlParser(object):
  """Defines an IDL parser that uses PLY to parse it.

  Each member that starts with p_ is a parse rule.  The docstring describes the
  rule.  The |p| argument is the tokens that were read.  p[1] is the first
  element that was patched. For example:

  Indexer : IDENTIFIER '.' IDENTIFIER
          | IDENTIFIER '[' Exp ']'

  For the above rule, p[1] is the first token, so in either case, it will be the
  IDENTIFIER.  p[2] will either be a string '.' or a string '[' depending on
  which version was matched. For tokens (e.g. '.' or IDENTIFIER), the value in
  |p| will be the string value (the |value| field from the token).

  The return value can be anything.  When that rule result is given to another
  rule, that value is passed as-is.  In the example above, if the Exp rule
  returned a tuple, then that tuple would be in p[3] to the Indexer rule.

  The naming style is that all terminals (i.e. things returned from the lexer)
  are given names in ALL_CAPS; non-terminals (i.e. rules defined here) use
  CamelCase.
  """

  def __init__(self, lexer_=None):
    self.errors = []
    self.lexer = lexer_ or lexer.IdlLexer()
    self.tokens = self.lexer.tokens
    self.yacc = yacc.yacc(
        module=self, tabmodule=None, optimize=0, debug=0, write_tables=0)

  def parse(self, name, contents):
    """Parses the given IDL code into a Results object."""
    try:
      self.errors = []
      self.lexer.set_contents(name, contents)
      ret = self.yacc.parse(lexer=self.lexer, tracking=True)
    except SyntaxError as e:
      # Place this error first since we can't continue parsing with it; but
      # keep any errors we saw already.
      self.errors = [e] + self.errors
    if self.errors:
      raise IdlSyntaxError(self.errors)
    return types.Results(types=ret)

  # Common rules and helpers ---------------------------------------------------
  # For some reason, even though this rule is first, the decorators are
  # confusing PLY and makes it think this isn't the start.
  start = 'Definitions'
  @_rule
  def p_Definitions(self, p):
    r"""Definitions : Definition Definitions
                    | Empty"""
    if len(p) > 2:
      return [p[1]] + p[2]
    else:
      return []

  @_rule
  def p_Empty(self, _):
    r"""Empty :"""
    return None

  @_rule
  def p_MaybeDoc(self, p):
    r"""MaybeDoc : DOCSTRING
                 | Empty"""
    return p[1]

  def p_error(self, t):
    """Called when an error occurs."""
    if t:
      line = t.lineno
      pos = t.lexpos
      prev = self.yacc.symstack[-1]
      if t.type == 'DOCSTRING':
        msg = ('This treats doc-strings (/** */) special and are only allowed '
               'before definitions or members')
      elif isinstance(prev, lex.LexToken):
        msg = 'Unexpected "%s" after "%s"' % (
            self._token_to_str(t), self._token_to_str(prev))
      else:
        msg = 'Unexpected "%s"' % t.value
    else:
      prev = self.lexer.last
      line = prev.lineno
      pos = prev.lexpos
      msg = 'Unexpected end of file after "%s"' % prev.value
    self._add_error(msg, line, pos)

  # Top-level rules ------------------------------------------------------------
  @_rule
  def p_Definition(self, p):
    r"""Definition : Dictionary"""
    # TODO: Add remaining definition types (e.g. 'interface', 'mixin', etc.)
    return p[1]

  @_rule
  def p_Dictionary(self, p):
    r"""Dictionary : MaybeDoc DICTIONARY IDENTIFIER '{' DictionaryMembers '}' ';'"""
    # TODO: Add support for inheritance.
    return types.Dictionary(name=p[3], attributes=p[5], doc=p[1])

  @_rule
  def p_Dictionary_error(self, p):
    r"""Dictionary : MaybeDoc DICTIONARY IDENTIFIER '{' error '}' ';'"""
    return types.Dictionary(name=p[3], attributes=[], doc=p[1])

  @_rule
  def p_DictionaryMembers(self, _):
    r"""DictionaryMembers :"""
    # TODO: Add support for dictionary members.
    return []

  # Helpers functions ----------------------------------------------------------
  def _add_error(self, message, line, offset):
    """Adds a new error to the error list."""
    line_text = self.lexer.get_line(offset)
    col = self.lexer.get_col(offset)
    self.errors.append(SyntaxError(
        message, (self.lexer.file_name, line, col, line_text)))

  def _token_to_str(self, t):
    """Gets a string representation of a token for errors."""
    special_tokens = {
        'STRING_LITERAL', 'DOCSTRING', 'FLOAT_LITERAL', 'INTEGER_LITERAL',
    }
    if t.type in special_tokens:
      return t.type
    else:
      return t.value


def ParseFile(name, contents):
  """Parses the given IDL file."""
  parser = IdlParser()
  return parser.parse(name, contents)
