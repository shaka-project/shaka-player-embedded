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
# pylint: disable=g-doc-exception
# pylint: disable=g-doc-return-or-yield
# pylint: disable=g-docstring-missing-newline
# pylint: disable=g-no-space-after-docstring-summary
# pylint: disable=g-short-docstring-punctuation
# pylint: disable=invalid-name
# pylint: disable=line-too-long

from __future__ import print_function

import functools
import sys

from ply import lex
from ply import yacc
from webidl import lexer
from webidl import types


__all__ = [
    'Features', 'Options', 'IdlSyntaxError', 'ExtensionError', 'IdlParser',
    'ParseFile',
]


if sys.version_info[0] == 2:
  _number_types = (float, int, long)
else:
  _number_types = (float, int)


# Don't use an enum here so an app can use string literals if they want.
class Features(object):
  """Defines possible features."""
  # Dictionaries can appear; this is assumed if any dictionary-* features are
  # present.
  DICTIONARY = 'dictionary'
  # Dictionary members can be marked as 'required'.
  DICTIONARY_REQUIRED = 'dictionary-required'
  # Dictionary members can have a default value given.
  DICTIONARY_DEFAULT = 'dictionary-default'


class Options(object):
  """Defines options for what is allowed in the code.

  This is used to indicate what features are allowed to be included in the IDL
  code.  This allows specifying what features are supported by your code
  instead of checking for invalid fields/types after parsing is done.  This also
  will raise a SyntaxError at the unsupported locations to make debugging
  easier.

  If the Options object is given, then only those features are supported. For
  example, this indicates that dictionaries are supported with default field
  values:

    Options('dictionary-default')
    Options(Features.DICTIONARY_DEFAULT)
  """

  # If any of the features given have a prefix of item[0], it results in the
  # feature item[1] being added.
  _ASSUMED_PREFIXES = [
      ('dictionary-', 'dictionary'),
  ]

  def __init__(self, *args):
    possible_features = Options._all_features()
    missing = set(args) - possible_features
    if missing:
      raise ValueError('Unknown feature(s) given: ' + ','.join(missing))

    self.features = set(args)
    for prefix, feature in Options._ASSUMED_PREFIXES:
      if any(item.startswith(prefix) for item in self.features):
        self.features.add(feature)

  @classmethod
  def all(cls):
    return cls(*Options._all_features())

  @staticmethod
  def _all_features():
    return {v for k, v in Features.__dict__.items() if k[0] != '_'}

  def has_feature(self, feature):
    """Returns whether the given feature is supported."""
    if feature not in Options._all_features():
      raise ValueError('Unknown feature given: ' + feature)
    return feature in self.features


class IdlSyntaxError(SyntaxError):
  """Defines a SyntaxError that may contain multiple errors."""

  def __init__(self, errors):
    assert errors, 'Must provide at least one error'
    super(IdlSyntaxError, self).__init__(*errors[0].args)
    self.inner_errors = errors


class ExtensionError(SyntaxError):
  """Defines an error when creating an extension object."""

  def __init__(self, error, data):
    super(ExtensionError, self).__init__(str(error), data)
    self.inner = error


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

  def __init__(self, lexer_=None, options=None, extensions=None):
    if extensions:
      temp = [(ext.name, kind) for ext in extensions for kind in ext.kinds]
      if len(set(temp)) != len(temp):
        raise RuntimeError('Multiple extensions with same name and kind')

    self.errors = []
    self.options = options or Options.all()
    self.lexer = lexer_ or lexer.IdlLexer()
    self.extensions = extensions or []
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
    r"""Dictionary : MaybeDoc ExtendedAttributeList DICTIONARY IDENTIFIER '{' DictionaryMembers '}' ';'"""
    # TODO: Add support for inheritance.
    self._check_options(p, 3, Features.DICTIONARY)
    debug = self._get_debug(p, 3)
    docDebug = self._get_debug(p, 1) if p[1] else None
    return types.Dictionary(
        name=p[4], attributes=p[6], doc=p[1], debug=debug, docDebug=docDebug,
        extensions=[e[0] for e in p[2]])

  @_rule
  def p_Dictionary_error(self, p):
    r"""Dictionary : MaybeDoc ExtendedAttributeList DICTIONARY IDENTIFIER '{' error '}' ';'"""
    self._check_options(p, 3, Features.DICTIONARY)
    return types.Dictionary(
        name=p[4], attributes=[], doc=p[1], debug=None, docDebug=None,
        extensions=[ext[0] for ext in p[2]])

  @_rule
  def p_DictionaryMembers(self, p):
    r"""DictionaryMembers : DictionaryMember DictionaryMembers
                          | Empty"""
    if len(p) == 3:
      return [p[1]] + p[2]
    else:
      return []

  @_rule
  def p_DictionaryMember(self, p):
    r"""DictionaryMember : MaybeDoc ExtendedAttributeList DictionaryMemberRest"""
    # TODO: Add support for extended attributes.
    docDebug = self._get_debug(p, 1) if p[1] else None
    return p[3]._replace(doc=p[1], docDebug=docDebug)

  @_rule
  def p_DictionaryMemberRest(self, p):
    r"""DictionaryMemberRest : REQUIRED TypeWithExtendedAttributes IDENTIFIER Default ';'
                             | Type IDENTIFIER Default ';'"""
    debug = self._get_debug(p, 1)
    if len(p) > 5:
      self._check_options(p, 1, Features.DICTIONARY_REQUIRED)
      if p[4] is not None:
        self._check_options(p, 4, Features.DICTIONARY_DEFAULT)

      return types.Attribute(
          name=p[3], type=p[2], default=p[4], is_required=True, doc=None,
          debug=debug, docDebug=None)
    else:
      if p[3] is not None:
        self._check_options(p, 3, Features.DICTIONARY_DEFAULT)

      return types.Attribute(
          name=p[2], type=p[1], default=p[3], is_required=False, doc=None,
          debug=debug, docDebug=None)

  # Types ----------------------------------------------------------------------
  @_rule
  def p_TypeWithExtendedAttributes(self, p):
    r"""TypeWithExtendedAttributes : ExtendedAttributeList Type"""
    # TODO: Add extended attribute support.
    return p[2]

  @_rule
  def p_Type(self, p):
    r"""Type : SingleType
             | UnionType Null"""
    if len(p) == 2:
      return p[1]
    else:
      return p[1]._replace(nullable=p[2])

  @_rule
  def p_SingleType(self, p):
    r"""SingleType : NonAnyType
                   | ANY"""
    if p[1] != 'any':
      return p[1]
    else:
      return types.IdlType(name='any', nullable=False, element_type=None)

  @_rule
  def p_UnionType(self, p):
    r"""UnionType : '(' ')'"""
    # TODO: Add union type support.
    raise SyntaxError('Union types not supported')

  @_rule
  def p_NonAnyType(self, p):
    r"""NonAnyType : PrimitiveType Null
                   | StringType Null
                   | IDENTIFIER Null
                   | PROMISE '<' ReturnType '>'
                   | SEQUENCE '<' TypeWithExtendedAttributes '>' Null
                   | FROZENARRAY '<' TypeWithExtendedAttributes '>' Null
                   | RECORD '<' StringType ',' TypeWithExtendedAttributes '>' Null"""
    # TODO: Add support for record<string, T>.
    # Unlike the official grammar definition, this doesn't include entries like
    # "Object Null"; these will be handled by the generic "IDENTIFIER Null".
    if len(p) == 3:
      if isinstance(p[1], types.IdlType):
        return p[1]._replace(nullable=p[2])
      else:
        return types.IdlType(name=p[1], nullable=p[2], element_type=None)
    elif len(p) == 5:
      return types.IdlType(name='Promise', nullable=False, element_type=p[3])
    elif len(p) == 6:
      return types.IdlType(name=p[1], nullable=p[5], element_type=p[3])
    else:
      assert len(p) == 8
      return types.IdlType(name=p[1], nullable=p[7], element_type=(p[3], p[5]))

  @_rule
  def p_ReturnType(self, p):
    r"""ReturnType : Type
                   | VOID"""
    if p[1] != 'void':
      return p[1]
    else:
      return types.IdlType(name='void', nullable=False, element_type=None)

  @_rule
  def p_PrimitiveType(self, p):
    r"""PrimitiveType : UNSIGNED LONG LONG
                      | UNSIGNED LONG
                      | UNSIGNED SHORT
                      | LONG LONG
                      | LONG
                      | SHORT
                      | UNRESTRICTED FLOAT
                      | UNRESTRICTED DOUBLE
                      | FLOAT
                      | DOUBLE
                      | BOOLEAN
                      | BYTE
                      | OCTET"""
    return types.IdlType(
        name=' '.join(p[1:]), nullable=False, element_type=None)

  @_rule
  def p_StringType(self, p):
    r"""StringType : BYTESTRING
                   | DOMSTRING
                   | USVSTRING"""
    return types.IdlType(name=p[1], nullable=False, element_type=None)

  @_rule
  def p_Null(self, p):
    r"""Null : '?'
             | Empty"""
    return p[1] == '?'

  # Argument list --------------------------------------------------------------
  @_rule
  def p_ArgumentList(self, p):
    r"""ArgumentList : Argument Arguments
                     | Empty"""
    if len(p) > 2:
      if p[1].optional and p[2] and not p[2][0].optional:
        self._add_error(
            'Optional arguments must be last', p.lineno(2), p.lineno(2))
      elif p[1].is_variadic and p[2]:
        self._add_error(
            'Variadic argument must be last', p.lineno(2), p.lineno(2))
      return [p[1]] + p[2]
    else:
      return []

  @_rule
  def p_Arguments(self, p):
    r"""Arguments : ',' Argument Arguments
                  | Empty"""
    if len(p) == 4:
      if p[2].optional and p[3] and not p[3][0].optional:
        self._add_error(
            'Optional arguments must be last', p.lineno(3), p.lineno(3))
      elif p[2].is_variadic and p[3]:
        self._add_error(
            'Variadic argument must be last', p.lineno(3), p.lineno(3))
      return [p[2]] + p[3]
    else:
      return []

  @_rule
  def p_Argument(self, p):
    r"""Argument : ExtendedAttributeList OPTIONAL TypeWithExtendedAttributes ArgumentName Default
                 | ExtendedAttributeList Type Ellipsis ArgumentName"""
    # TODO: Add extended attribute support.
    if len(p) > 5:
      return types.Argument(
          name=p[4], type=p[3], optional=True, is_variadic=False, default=p[5])
    else:
      return types.Argument(
          name=p[4], type=p[2], optional=False, is_variadic=p[3], default=None)

  @_rule
  def p_Ellipsis(self, p):
    r"""Ellipsis : ELLIPSIS
                 | Empty"""
    return p[1] == '...'

  @_rule
  def p_ArgumentName(self, p):
    r"""ArgumentName : IDENTIFIER
                     | ATTRIBUTE
                     | CALLBACK
                     | CONST
                     | DELETER
                     | DICTIONARY
                     | ENUM
                     | GETTER
                     | INCLUDES
                     | INHERIT
                     | INTERFACE
                     | ITERABLE
                     | MAPLIKE
                     | NAMESPACE
                     | PARTIAL
                     | REQUIRED
                     | SETLIKE
                     | SETTER
                     | STATIC
                     | STRINGIFIER
                     | TYPEDEF
                     | UNRESTRICTED"""
    return p[1]

  # Extended attributes --------------------------------------------------------
  # Unlike the other rules, the ExtendedAttribute* rules don't return the AST
  # value itself; instead they return a tuple of (extension, line, offset).
  # This allows the caller to give the correct position for each invalid
  # extension.  ExtendedAttributeList and ExtendedAttributes return a list of
  # tuples.
  @_rule
  def p_ExtendedAttributeList(self, p):
    r"""ExtendedAttributeList : '[' ExtendedAttribute ExtendedAttributes ']'
                              | Empty"""
    if len(p) > 2:
      return [p[2]] + p[3]
    else:
      return []

  @_rule
  def p_ExtendedAttributeList_error(self, p):
    r"""ExtendedAttributeList : '[' error ']'"""
    return []

  @_rule
  def p_ExtendedAttributes(self, p):
    r"""ExtendedAttributes : ',' ExtendedAttribute ExtendedAttributes
                           | Empty"""
    if len(p) > 2:
      return [p[2]] + p[3]
    else:
      return []

  @_rule
  def p_ExtendedAttribute(self, p):
    r"""ExtendedAttribute : IDENTIFIER
                          | IDENTIFIER '=' IDENTIFIER
                          | IDENTIFIER '(' ArgumentList ')'
                          | IDENTIFIER '=' '(' IdentifierList ')'
                          | IDENTIFIER '=' IDENTIFIER '(' ArgumentList ')'"""
    if len(p) == 2:
      kind = types.ExtensionKind.NO_ARGS
      args = {}
    elif len(p) == 4:
      kind = types.ExtensionKind.IDENT
      args = {'arg': p[3]}
    elif len(p) == 5:
      kind = types.ExtensionKind.ARG_LIST
      args = {'args': p[3]}
    elif len(p) == 6:
      kind = types.ExtensionKind.IDENT_LIST
      args = {'args': p[4]}
    else:
      assert len(p) == 7
      kind = types.ExtensionKind.NAMED_ARG_LIST
      args = {'argName': p[3], 'args': p[5]}

    # Find an extension with the correct name and kind.  If there isn't one with
    # the correct kind, find one with the correct name to give a better error
    # message.
    extension = None
    for cur in self.extensions:
      if cur.name == p[1]:
        if (extension is None or
            (kind not in extension.kind and kind in cur.kind)):
          extension = cur

    if not extension or kind not in extension.kinds:
      if not extension:
        message = 'Unknown extension [%s]' % p[1]
      elif len(extension.kinds) == 1:
        names = {
            types.ExtensionKind.NO_ARGS: 'no arguments',
            types.ExtensionKind.ARG_LIST: 'an argument list',
            types.ExtensionKind.NAMED_ARG_LIST: 'a named argument list',
            types.ExtensionKind.IDENT: 'an identifier',
            types.ExtensionKind.IDENT_LIST: 'an identifier list',
        }
        message = '[%s] should have %s but was given %s' % (
            p[1], names[extension.kinds[0]], names[kind])
      else:
        message = '[%s] is not in an allowed form' % p[1]
      self._add_error(message, p.lineno(1), p.lexpos(1))
      return (None, p.lineno(1), p.lexpos(1))

    try:
      return (extension(**args), p.lineno(1), p.lexpos(1))
    except Exception as e:  # pylint: disable=broad-except
      self._add_error(e, p.lineno(1), p.lexpos(1), except_type=ExtensionError)
      return (None, p.lineno(1), p.lexpos(1))

  @_rule
  def p_IdentifierList(self, p):
    r"""IdentifierList : IDENTIFIER Identifiers"""
    return [p[1]] + p[2]

  @_rule
  def p_Identifiers(self, p):
    r"""Identifiers : ',' IDENTIFIER Identifiers
                    | Empty"""
    if len(p) > 2:
      return [p[2]] + p[3]
    else:
      return []

  def _check_extensions(self, extensions, locations):
    """Checks the given extensions are valid, and adds errors otherwise."""
    for ext, line, offset in extensions:
      if not ext or any(l in ext.locations for l in locations):
        continue
      self._add_error(
          '[%s] is not valid in this context' % ext.name, line, offset)

  # Constants ------------------------------------------------------------------
  @_rule
  def p_Default(self, p):
    r"""Default : '=' DefaultValue
                | Empty"""
    if len(p) > 2:
      return p[2]
    else:
      return None

  @_rule
  def p_DefaultValue(self, p):
    r"""DefaultValue : ConstValue
                     | STRING_LITERAL
                     | '[' ']'"""
    if len(p) == 2:
      # STRING_LITERAL values are converted by the lexer; ConstValue also
      # returns the converted value.
      return p[1]
    else:
      return []

  @_rule
  def p_ConstValue(self, p):
    r"""ConstValue : TRUE
                   | FALSE
                   | FLOAT_LITERAL
                   | NAN
                   | INFINITY
                   | '-' INFINITY
                   | INTEGER_LITERAL
                   | NULL"""
    if len(p) == 3:
      return float('-inf')
    elif isinstance(p[1], _number_types):
      # FLOAT_LITERAL and INTEGER_LITERAL values are converted by the lexer.
      return p[1]
    else:
      map_ = {
          'true': True,
          'false': False,
          'NaN': float('nan'),
          'Infinity': float('inf'),
          'null': types.IdlNull,
      }
      return map_[p[1]]

  # Helpers functions ----------------------------------------------------------
  def _add_error(self, message, line, offset, except_type=SyntaxError):
    """Adds a new error to the error list."""
    line_text = self.lexer.get_line(offset)
    col = self.lexer.get_col(offset)
    self.errors.append(except_type(
        message, (self.lexer.file_name, line, col, line_text)))

  def _check_options(self, p, idx, feature):
    """Checks that the given feature is allowed, and adds an error otherwise."""
    if self.options.has_feature(feature):
      return
    self._add_error('Feature "%s" is not allowed by options' % feature,
                    p.lineno(idx), p.lexpos(idx))

  def _get_debug(self, p, idx):
    """Gets a DebugInfo for the given token."""
    offset = p.lexpos(idx)
    return types.DebugInfo(lineno=p.lineno(idx), col=self.lexer.get_col(offset),
                           line=self.lexer.get_line(offset))

  def _token_to_str(self, t):
    """Gets a string representation of a token for errors."""
    special_tokens = {
        'STRING_LITERAL', 'DOCSTRING', 'FLOAT_LITERAL', 'INTEGER_LITERAL',
    }
    if t.type in special_tokens:
      return t.type
    else:
      return t.value


def ParseFile(name, contents, options=None, extensions=None):
  """Parses the given IDL file."""
  parser = IdlParser(options=options, extensions=extensions)
  return parser.parse(name, contents)
