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

from __future__ import print_function

import collections
import string
import sys

try:
  import enum
except ImportError:
  print('Requires the "enum34" pip polyfill to be installed', file=sys.stderr)
  raise


class IdlTokenType(enum.Enum):
  """Defines an enumeration with the possible token types."""
  BEGIN_INTERFACE = '{'
  END_INTERFACE = '}'
  BEGIN_ARGS = '('
  END_ARGS = ')'
  COMMA = ','
  SEMI_COLON = ';'
  NULLABLE = '?'
  BEGIN_TEMPLATE = '<'
  END_TEMPLATE = '>'
  BEGIN_EXTENSION = '['
  END_EXTENSION = ']'
  EQUALS = '='

  ELLIPSIS = '...'
  DICTIONARY = 'dictionary'
  OPTIONAL = 'optional'

  # The |value| field contains the parsed string contents.
  STRING_LITERAL = '<string>'
  # The |value| field contains the parsed value as a number.
  NUMBER_LITERAL = '<number>'
  # The |value| field contains the string identifier value.
  IDENTIFIER = '<identifier>'


IdlToken = collections.namedtuple('IdlToken', ('type', 'value', 'length',
                                               'doc'))


class StringReader(object):
  """This reads from a string and tracks the current line/column."""

  def __init__(self, data):
    self._data = data
    self._pos = 0

    self.line = 1
    self.col = 1

  def eof(self):
    """Returns whether the reader is at end-of-file."""
    return self._pos >= len(self._data)

  def read(self, count):
    """Reads the given number of characters from the input and returns them."""
    ret = self.peek(count)
    self._pos += len(ret)
    self.line += ret.count('\n')
    if '\n' in ret:
      self.col = len(ret) - ret.rindex('\n')
    else:
      self.col += len(ret)
    return ret

  def peek(self, count):
    """Returns a string of the given number of characters from the input."""
    return self._data[self._pos:self._pos+count]

  def peek_until(self, substr, skip=0):
    """Returns a string until the given substring is found."""
    idx = self._data.find(substr, self._pos + skip)
    if idx < 0:
      idx = len(self._data)
    return self._data[self._pos:idx+len(substr)]

  def peek_in_character_set(self, choices, skip=0):
    """Returns the largest string of characters from the given set."""
    count = skip
    while (self._pos + count < len(self._data) and
           self._data[self._pos + count] in choices):
      count += 1
    return self._data[self._pos:self._pos+count]

  def get_line(self):
    """Returns the line that contains the current position."""
    prev = self._data.rfind('\n', 0, self._pos)
    nxt = self._data.find('\n', self._pos)
    if nxt < 0:
      nxt = len(self._data)
    return self._data[prev+1:nxt]


class IdlTokenizer(object):
  """This reads input text and converts it to a series of tokens.

  This will handle skipping whitespace and comments and creates token objects
  based on the values received.
  """

  def __init__(self, filename, data):
    """Creates a new tokenizer.

    Args:
      filename: The name of the file this is reading from.
      data: The string data to read.
    """
    self._file = filename
    self._reader = StringReader(data)
    # The next token in the stream, or None if it hasn't been parsed yet.
    self._peek = None
    self._doc = None

  def eof(self):
    """Returns whether the tokenizer is at end-of-file."""
    return self._reader.eof()

  def raise_error(self, message):
    """Raises a SyntaxError at the current position."""
    raise SyntaxError(message, (self._file, self._reader.line, self._reader.col,
                                self._reader.get_line()))

  def expect(self, token_type):
    """Expects that the next token is the given type and throws otherwise."""
    # Use peek() so the exception uses the correct position.
    token = self.peek()
    if not token:
      self.raise_error('Unexpected EOF looking for "%s"' % token_type.name)
    if token.type != token_type:
      self.raise_error(
          'Expecting "%s", got "%s"' % (token_type.name, token.type.name))
    return self.next()

  def read_if_type(self, token_type):
    """If the next token is the given type, read it.

    Returns:
      Whether the token was read.
    """
    token = self.peek()
    if not token or token.type != token_type:
      return False
    self.next()
    return True

  def peek_is_type(self, token_type):
    """Returns whether the next token is the given type."""
    token = self.peek()
    return token and token.type == token_type

  def next(self):
    """Reads and returns the next token in the input."""
    ret = self.peek()
    if not ret:
      return None
    self._reader.read(ret.length)
    self._peek = None
    return ret

  def peek(self):
    """Returns the next token in the input but doesn't read it."""
    if not self._peek:
      self._skip_whitespace_and_comments()
      ch = self._reader.peek(1)
      if not ch:
        return None

      if any(ch == t.value for t in IdlTokenType):
        self._set_peek(ch)
      elif ch == '"':
        self._set_peek_string()
      elif ch == '.' and self._reader.peek(3) == '...':
        self._set_peek('...')
      elif ch in string.digits + '-.':
        self._set_peek_number()
      elif ch == '_' or ch in string.ascii_letters:
        ident = self._peek_token()
        if ident == 'NaN':
          self._set_peek(
              float('nan'), length=3, type_=IdlTokenType.NUMBER_LITERAL)
        elif ident == 'Infinity':
          self._set_peek(
              float('inf'), length=8, type_=IdlTokenType.NUMBER_LITERAL)
        else:
          self._set_peek(ident)
      else:
        self.raise_error('Unexpected character "%s"' % ch)
    return self._peek

  def _skip_whitespace_and_comments(self):
    """Skips whitespace and comments at the current position."""
    extra = ''
    while True:
      ch = self._reader.peek(1)
      if not ch:
        break

      if ch in string.whitespace:
        if ch == '\n':
          extra = ''
        else:
          extra += ch
        self._reader.read(1)
      elif ch == '/':
        ch = self._reader.peek(2)
        if ch == '//':
          comment = self._reader.peek_until('\n')
          self._reader.read(len(comment))
          extra = ''
        elif ch == '/*':
          doc = self._reader.peek_until('*/', skip=2)
          if doc == '/*/' or not doc.endswith('*/'):
            self.raise_error('Unexpected EOF in comment')
          if doc != '/**/' and doc[:3] == '/**':
            self._doc = extra + doc
          self._reader.read(len(doc))
          extra = ''
        else:
          break
      else:
        break

  def _peek_token(self, allow_dot=False, skip=0):
    """Returns the next distinct token.

    A token is a series of letters, numbers, dashes, underscores, or optionally
    dots (e.g. for numbers).
    """
    letters_or_digits = '-_' + string.ascii_letters + string.digits
    if allow_dot:
      letters_or_digits += '.'
    return self._reader.peek_in_character_set(letters_or_digits, skip=skip)

  def _set_peek_string(self):
    """Finds the end of the string at the current position and sets the peek."""
    word = self._reader.peek_until('"', skip=1)
    if not word.endswith('"') or word == '"':
      self.raise_error('Unexpected EOF in string literal')
    if '\n' in word:
      self.raise_error('Newline in string literal')
    self._set_peek(
        word[1:-1], length=len(word), type_=IdlTokenType.STRING_LITERAL)

  def _set_peek_number(self):
    """Finds the end of the number at the current position and sets the peek."""
    word = self._peek_token(allow_dot=True)
    if word == '-Infinity':
      self._set_peek(float('-inf'), length=9, type_=IdlTokenType.NUMBER_LITERAL)
      return

    try:
      if (word.startswith('0x') or word.startswith('0X') or
          word.startswith('-0x') or word.startswith('-0X')):
        value = int(word, 16)
      elif word.startswith('0'):
        value = int(word, 8)
      else:
        value = float(word)
    except ValueError:
      self.raise_error('Invalid number format')
    self._set_peek(
        value, length=len(word), type_=IdlTokenType.NUMBER_LITERAL)

  def _set_peek(self, value, length=None, type_=None):
    """Sets the peek value to a token with the given value."""
    if type_ is None:
      for entry in IdlTokenType:
        if entry.value == value:
          type_ = entry
          break
      else:
        type_ = IdlTokenType.IDENTIFIER
    if length is None:
      length = len(value)
    self._peek = IdlToken(type=type_, value=value, length=length, doc=self._doc)
    self._doc = None
