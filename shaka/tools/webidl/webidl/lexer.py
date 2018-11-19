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

"""Defines a lex tokenizer for IDL files.

This uses PLY for the basis:
    http://www.dabeaz.com/ply/
This parses WebIDL syntax:
    https://heycam.github.io/webidl/#idl-grammar
"""

# pylint: disable=invalid-name

from __future__ import print_function

from ply import lex


__all__ = ['IdlLexer']


class IdlLexer(object):
  """A PLY lexer that reads WebIDL syntax.

  This has two special fields: tokens and literals.  Other members that start
  with 't_' are special and define how to parse different tokens.  These
  parsers are invoked in the order they are defined, so it is important that
  early ones override later ones.
  """

  def __init__(self):
    self.lex = lex.lex(object=self, optimize=0)
    self.file_name = None
    self.contents = None
    self.last = None

  @property
  def lineno(self):
    try:
      return self.lex.lineno
    except AttributeError:
      # This can be called before the |lex| value is set; ignore in this case.
      return 1

  @property
  def lexpos(self):
    try:
      return self.lex.lexpos
    except AttributeError:
      # This can be called before the |lex| value is set; ignore in this case.
      return 1

  def set_contents(self, name, contents):
    """Resets the lexer to read from the given text."""
    self.lex.lineno = 1
    self.lex.input(contents)
    self.file_name = name
    self.contents = contents
    self.last = None

  def token(self):
    ret = self.lex.token()
    if ret:
      self.last = ret
    return ret

  def get_col(self, pos):
    """Gets the column index of the given lexpos."""
    return pos - self.contents.rfind('\n', 0, pos)

  def get_line(self, pos):
    """Gets the line of text that is at position |pos|."""
    prev = self.contents.rfind('\n', 0, pos)
    nxt = self.contents.find('\n', pos)
    if nxt < 0:
      nxt = len(self.contents)
    return self.contents[prev+1:nxt]

  _keywords = (
      'any',
      'attribute',
      'callback',
      'const',
      'deleter',
      'dictionary',
      'enum',
      'false',
      'getter',
      'includes',
      'Infinity',
      'inherit',
      'interface',
      'iterable',
      'maplike',
      'namespace',
      'NaN',
      'null',
      'optional',
      'partial',
      'required',
      'sequence',
      'setlike',
      'setter',
      'static',
      'stringifier',
      'true',
      'typedef',
      'void',

      'boolean',
      'byte',
      'double',
      'float',
      'long',
      'short',
      'octet',
      'unrestricted',
      'unsigned',
      'record',

      'ByteString',
      'DOMString',
      'FrozenArray',
      'Promise',
      'USVString',
  )

  literals = '.(){}[]<>,;:=?-'
  tokens = (
      'FLOAT_LITERAL',
      'INTEGER_LITERAL',
      'STRING_LITERAL',

      'ELLIPSIS',
      'DOCSTRING',
      'IDENTIFIER',
  ) + tuple(k.upper() for k in _keywords)

  def t_error(self, t):
    msg = 'Unexpected character "%s"' % t.value[0]
    line = self.get_line(t.lexpos)
    col = self.get_col(t.lexpos)
    raise SyntaxError(msg, (self.file_name, t.lineno, col, line))

  t_ignore = ' \t'
  t_ELLIPSIS = r'\.\.\.'

  @lex.TOKEN(r'-?(([0-9]+\.[0-9]*|[0-9]*\.[0-9]+)([Ee][+-]?[0-9]+)?|'
             r'   [0-9]+[Ee][+-]?[0-9]+)')
  def t_FLOAT_LITERAL(self, t):
    t.value = float(t.value)
    return t

  @lex.TOKEN('-?([1-9][0-9]*|0[Xx][0-9A-Fa-f]+|0[0-7]*)')
  def t_INTEGER_LITERAL(self, t):
    if t.value.lower().startswith('0x') or t.value.lower().startswith('-0x'):
      t.value = int(t.value, 16)
    elif t.value.startswith('0') or t.value.startswith('-0'):
      t.value = int(t.value, 8)
    else:
      t.value = int(t.value, 10)
    return t

  @lex.TOKEN(r'"[^"]*"')
  def t_STRING_LITERAL(self, t):
    t.value = t.value[1:-1]
    self.lex.lineno += t.value.count('\n')
    return t

  @lex.TOKEN(r'\n+')
  def t_newline(self, t):
    self.lex.lineno += len(t.value)

  @lex.TOKEN(r'/\*\*(.|\n)+?\*/')
  def t_DOCSTRING(self, t):
    self.lex.lineno += t.value.count('\n')
    return t

  @lex.TOKEN(r'(/\*(.|\n)*?\*/)|(//.*\n)')
  def t_COMMENT(self, t):
    self.lex.lineno += t.value.count('\n')

  @lex.TOKEN(r'_?[A-Za-z][0-9A-Za-z_-]*')
  def t_IDENTIFIER(self, t):
    if t.value in self._keywords:
      t.type = t.value.upper()
    else:
      t.type = 'IDENTIFIER'
    return t
