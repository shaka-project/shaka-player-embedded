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

"""Defines a parser for IDL files."""

from __future__ import print_function

from webidl import types
from webidl.idl_tokenizer import IdlTokenizer
from webidl.idl_tokenizer import IdlTokenType


class IdlParser(object):
  """Defines a parser that parses IDL code.

  This can be subclasses to change the behavior of different parse methods.  The
  read_* methods are called when the respective token is seen; it should
  read all the tokens for that item and should returned the parsed version of
  it.
  """

  def __init__(self, extension_types=None):
    """Creates a new IdlParser type.

    Args:
      extension_types: A list of types that subclass Extension.  These are
        used to determine the valid extended attributes while parsing.  This
        can be None for no extensions.
    """
    self._reader = None
    self._extension_types = extension_types or []

  def parse_file(self, path, body):
    """Parses the given IDL file.

    Args:
      path: The string path to the IDL file.
      body: The text contents of the IDL file.

    Returns:
      An Results object for the results of the parse.
    """
    self._reader = IdlTokenizer(path, body)

    ret = []
    while self._reader.peek():
      if self._reader.peek_is_type(IdlTokenType.DICTIONARY):
        ret.append(self.read_dictionary())
      else:
        self._reader.raise_error(
            'Invalid top-level token "%s"' % self._reader.peek().type.name)

    self._reader = None
    return types.Results(types=ret)

  def read_dictionary(self):
    """Reads an IDL dictionary definition.

    Returns:
      A Dictionary object.
    """
    # The tokenizer attaches the doc to the next token, so the type's doc is
    # actually on the 'dictionary' token.
    dict_doc = self._reader.expect(IdlTokenType.DICTIONARY).doc
    name = self._reader.expect(IdlTokenType.IDENTIFIER).value

    attributes = []
    self._reader.expect(IdlTokenType.BEGIN_INTERFACE)
    while (self._reader.peek() and
           self._reader.peek().type != IdlTokenType.END_INTERFACE):
      attr_doc = self._reader.peek().doc
      t = self.read_type()
      attr_name = self._reader.expect(IdlTokenType.IDENTIFIER).value
      attributes.append(types.Attribute(
          name=attr_name, type=t, default=None, is_required=False, doc=attr_doc,
          debug=None, docDebug=None))
      self._reader.expect(IdlTokenType.SEMI_COLON)

    self._reader.expect(IdlTokenType.END_INTERFACE)
    self._reader.expect(IdlTokenType.SEMI_COLON)
    return types.Dictionary(name=name, attributes=attributes, doc=dict_doc,
                            debug=None, docDebug=None)

  def read_type(self):
    """Reads an IDL type for an attribute or argument.

    Returns:
      An IdlType object.
    """
    type_name = self._reader.expect(IdlTokenType.IDENTIFIER).value

    if self._reader.read_if_type(IdlTokenType.BEGIN_TEMPLATE):
      if type_name != 'sequence':
        self._reader.raise_error('Can only use templates (<>) on sequence<T>.')
      element_type = self.read_type()
      self._reader.expect(IdlTokenType.END_TEMPLATE)
    else:
      element_type = None

    nullable = self._reader.read_if_type(IdlTokenType.NULLABLE)
    return types.IdlType(
        name=type_name, nullable=nullable, element_type=element_type)

  def read_arg_list(self):
    """Parses an IDL argument list.

    Returns:
      A list of Argument objects.
    """
    self._reader.expect(IdlTokenType.BEGIN_ARGS)
    ret = []
    if not self._reader.peek_is_type(IdlTokenType.END_ARGS):
      while True:
        optional = self._reader.read_if_type(IdlTokenType.OPTIONAL)
        variadic = False
        type_ = self.read_type()
        if optional:
          if self._reader.peek_is_type(IdlTokenType.ELLIPSIS):
            self._reader.raise_error('Cannot use "..." with optional arguments')
        else:
          variadic = self._reader.read_if_type(IdlTokenType.ELLIPSIS)

        arg_name = self._reader.expect(IdlTokenType.IDENTIFIER).value
        if optional:
          if self._reader.read_if_type(IdlTokenType.EQUALS):
            default = self.read_const(True)
          else:
            default = None
        else:
          if self._reader.peek_is_type(IdlTokenType.EQUALS):
            self._reader.raise_error(
                'Cannot have default values with non-optional argument')
          if any(i for i in ret if i.optional):
            self._reader.raise_error('Optional arguments must appear last')
          default = None

        ret.append(types.Argument(
            name=arg_name, type=type_, optional=optional, is_variadic=variadic,
            default=default))
        if self._reader.peek_is_type(IdlTokenType.COMMA):
          if variadic:
            self._reader.raise_error('An argument with "..." must appear last')
          else:
            self._reader.expect(IdlTokenType.COMMA)
        else:
          break

    self._reader.expect(IdlTokenType.END_ARGS)
    return ret

  def read_const(self, allow_obj):
    """Reads an IDL constant or default argument value.

    Args:
      allow_obj: Whether to allow objects as the constant.  If False, this will
        only allow booleans, numbers, and null.

    Returns:
      The parsed constant value.
    """
    if self._reader.peek_is_type(IdlTokenType.IDENTIFIER):
      token = self._reader.peek()
      if token.value == 'true':
        self._reader.next()
        return True
      elif token.value == 'false':
        self._reader.next()
        return False
      elif token.value == 'null':
        self._reader.next()
        return types.IdlNull
      else:
        self._reader.raise_error('Invalid constant value')
    if self._reader.peek_is_type(IdlTokenType.NUMBER_LITERAL):
      return self._reader.expect(IdlTokenType.NUMBER_LITERAL).value

    if allow_obj:
      if self._reader.read_if_type(IdlTokenType.BEGIN_EXTENSION):
        self._reader.expect(IdlTokenType.END_EXTENSION)
        return []
      if self._reader.peek_is_type(IdlTokenType.STRING_LITERAL):
        return self._reader.expect(IdlTokenType.STRING_LITERAL).value

    self._reader.raise_error('Expecting a constant')

  def _check_extension_kind(self, extension, actual):
    """Checks the extension kind is valid, and throws if not."""
    if actual == extension.kind:
      return
    names = {
        types.ExtensionKind.NO_ARGS: 'no arguments',
        types.ExtensionKind.ARG_LIST: 'an argument list',
        types.ExtensionKind.NAMED_ARG_LIST: 'a named argument list',
        types.ExtensionKind.IDENT: 'an identifier',
        types.ExtensionKind.IDENT_LIST: 'an identifier list',
    }
    fmt = '[%s] should have %s but was given %s'
    self._reader.raise_error(
        fmt % (extension.name, names[extension.kind], names[actual]))

  def read_extensions(self, locations):
    """Reads an IDL extension list.

    Args:
      locations: The ExtensionLocation for where this extension is.  There
        are cases where multiple locations apply.  For example, at the beginning
        of an interface member definition, the extensions could be for the
        member or for the return type.

    Returns:
      The list of extension objects.  The objects are constructed using the
      types from |extension_types|.
    """
    self._reader.expect(IdlTokenType.BEGIN_EXTENSION)
    extensions = []
    while True:
      extensions.append(self.read_extension(locations))
      if not self._reader.read_if_type(IdlTokenType.COMMA):
        break

    self._reader.expect(IdlTokenType.END_EXTENSION)
    return extensions

  def read_extension(self, locations):
    """Reads a single IDL extension.

    Args:
      locations: The ExtensionLocation for where this extension is.  There
        are cases where multiple locations apply.  For example, at the beginning
        of an interface member definition, the extensions could be for the
        member or for the return type.

    Returns:
      The extension object.  The object is constructed using the types from
      |extension_types|.
    """
    name = self._reader.expect(IdlTokenType.IDENTIFIER).value
    matched = [t for t in self._extension_types if t.name == name]
    assert len(matched) <= 1, 'Multiple extensions with the same name'
    if not matched or all(l not in matched[0].locations for l in locations):
      self._reader.raise_error(
          '[%s] is not a valid extension in this context' % name)

    if self._reader.peek_is_type(IdlTokenType.BEGIN_ARGS):
      # [Constructor(double x, double y)]
      self._check_extension_kind(matched[0], types.ExtensionKind.ARG_LIST)
      args = self.read_arg_list()
      return matched[0](args=args)
    elif self._reader.read_if_type(IdlTokenType.EQUALS):
      if self._reader.read_if_type(IdlTokenType.BEGIN_ARGS):
        # [Exposed=(Window, Worker)]
        self._check_extension_kind(matched[0], types.ExtensionKind.IDENT_LIST)
        names = []
        while True:
          names.append(self._reader.expect(IdlTokenType.IDENTIFIER).value)
          if not self._reader.read_if_type(IdlTokenType.COMMA):
            break
        self._reader.expect(IdlTokenType.END_ARGS)
        return matched[0](args=names)
      else:
        name = self._reader.expect(IdlTokenType.IDENTIFIER).value
        if self._reader.peek_is_type(IdlTokenType.BEGIN_ARGS):
          # [NamedConstructor=Image(DOMString src)]
          self._check_extension_kind(
              matched[0], types.ExtensionKind.NAMED_ARG_LIST)
          args = self.read_arg_list()
          return matched[0](argsName=name, args=args)
        else:
          # [PutForwards=name]
          if matched[0].kind == types.ExtensionKind.IDENT_LIST:
            return matched[0](args=[name])
          else:
            self._check_extension_kind(matched[0], types.ExtensionKind.IDENT)
            return matched[0](arg=name)
    else:
      # [Replaceable]
      self._check_extension_kind(matched[0], types.ExtensionKind.NO_ARGS)
      return matched[0]()
