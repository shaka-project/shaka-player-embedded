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

import collections
import re
import sys

from webidl.idl_tokenizer import IdlTokenizer
from webidl.idl_tokenizer import IdlTokenType

try:
  import enum  # pylint: disable=g-import-not-at-top
except ImportError:
  print('Requires the "enum34" pip polyfill to be installed', file=sys.stderr)
  raise


class IdlExtensionLocation(enum.Enum):
  """Defines possible locations an extension can be put."""
  # e.g. an 'interface' definition.
  DEFINITION = 'definition'
  # An annotated type (e.g. as an argument type).  Note this is different than
  # ARGUMENT.  If the location is ambiguous, it favors the TYPE; this means that
  # the extension will appear on the IdlType and not on the IdlArgument.
  TYPE = 'type'
  MEMBER = 'member'
  MIXIN_MEMBER = 'mixin member'
  NAMESPACE_MEMBER = 'namespace member'
  DICTIONARY_MEMBER = 'dictionary member'
  ARGUMENT = 'argument'


class IdlExtensionKind(enum.Enum):
  """Defines the type of extension this is.

  The type of extension defines the fields that exist on the object.  The
  constructor is given named arguments for each of the fields.
  """
  # This has no fields.
  # For example: [Replaceable]
  NO_ARGS = 'NoArgs'
  # - 'args': A list of IdlArgument objects for the args.
  # For example: [Constructor(double x, double y)]
  ARG_LIST = 'ArgList'
  # - 'argsName': The string name given.
  # - 'args': A list of IdlArgument objects for the args.
  # For example: [NamedConstructor=Image(DOMString src)]
  NAMED_ARG_LIST = 'NamedArgList'
  # - 'arg': The string identifier given.
  # For example: [PutForwards=name]
  IDENT = 'Ident'
  # - 'args': A list of strings for the identifiers given.
  # For example: [Exposed=(Window,Worker)]
  # Note this allows using [Exposed=Window] for a single item.
  IDENT_LIST = 'IdentList'


class IdlExtension(object):
  """Defines a base class for extensions.

  Derived classes are expected to define the following type fields:
  - name: A string name of the extension.
  - kind: An IdlExtensionKind for the kind of extension.
  - locations: A list of IdlExtensionLocation for where the extension can be.

  The constructor will be called with name-value pairs based on the kind of
  extension this is.
  """

  def __init__(self, **kwargs):
    for k, v in kwargs.items():
      setattr(self, k, v)


class IdlNull(object):
  """A signal type used to indicate a default value is "null"."""


class IdlArgument(collections.namedtuple(
    'IdlArgument', ('name', 'type', 'optional', 'is_variadic', 'default'))):
  """Defines an argument to a method.

  Properties:
    name: The string name of the argument.
    type: An IdlType object that describes the type of the argument.
    optional: Whether this argument is optional.
    is_variadic: Whether this argument is variadic; this will only be True for
      the last argument in the list.
    default: The default value of the argument.  This can be a number, string,
      boolean, empty list, an IdlNull object, or None (for no default).
  """
  __slots__ = ()


class IdlType(collections.namedtuple('IdlType',
                                     ('name', 'nullable', 'element_type'))):
  """Defines a type of an attribute or argument.

  Properties:
    name: The string name of the type.
    nullable: Whether the type can be null.
    element_type: If present, it is an IdlType that specifies the element type
      for an array.  In this case, |name| is always "sequence".
  """
  __slots__ = ()


class IdlAttribute(collections.namedtuple('IdlAttribute',
                                          ('name', 'type', 'doc'))):
  """Defines an attribute on a type.

  Properties:
    name: The string name of the attribute.
    type: An IdlType object defining the type of the attribute.
    doc: The string comment describing the attribute.
  """
  __slots__ = ()


class Dictionary(collections.namedtuple('Dictionary',
                                        ('name', 'attributes', 'doc'))):
  """Defines a dictionary definition.

  Properties:
    name: The string name of the dictionary.
    attributes: A list of IdlAttribute objects for the attributes in the dict.
    doc: The string comment describing the type.
  """
  __slots__ = ()


class IdlFile(collections.namedtuple('IdlFile', ('types',))):
  """Defines the results of parsing an IDL file.

  Properties:
    types: A list of types the file defines.  Will always be a Dictionary type.
  """
  __slots__ = ()


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
      extension_types: A list of types that subclass IdlExtension.  These are
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
      An IdlFile object for the results of the parse.
    """
    self._reader = IdlTokenizer(path, body)

    types = []
    while self._reader.peek():
      if self._reader.peek_is_type(IdlTokenType.DICTIONARY):
        types.append(self.read_dictionary())
      else:
        self._reader.raise_error(
            'Invalid top-level token "%s"' % self._reader.peek().type.name)

    self._reader = None
    return IdlFile(types=types)

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
      attributes.append(IdlAttribute(name=attr_name, type=t, doc=attr_doc))
      self._reader.expect(IdlTokenType.SEMI_COLON)

    self._reader.expect(IdlTokenType.END_INTERFACE)
    self._reader.expect(IdlTokenType.SEMI_COLON)
    return Dictionary(name=name, attributes=attributes, doc=dict_doc)

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
    return IdlType(name=type_name, nullable=nullable, element_type=element_type)

  def read_arg_list(self):
    """Parses an IDL argument list.

    Returns:
      A list of IdlArgument objects.
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

        ret.append(IdlArgument(
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
        return IdlNull()
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
        IdlExtensionKind.NO_ARGS: 'no arguments',
        IdlExtensionKind.ARG_LIST: 'an argument list',
        IdlExtensionKind.NAMED_ARG_LIST: 'a named argument list',
        IdlExtensionKind.IDENT: 'an identifier',
        IdlExtensionKind.IDENT_LIST: 'an identifier list',
    }
    fmt = '[%s] should have %s but was given %s'
    self._reader.raise_error(
        fmt % (extension.name, names[extension.kind], names[actual]))

  def read_extensions(self, locations):
    """Reads an IDL extension list.

    Args:
      locations: The IdlExtensionLocation for where this extension is.  There
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
      locations: The IdlExtensionLocation for where this extension is.  There
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
      self._check_extension_kind(matched[0], IdlExtensionKind.ARG_LIST)
      args = self.read_arg_list()
      return matched[0](args=args)
    elif self._reader.read_if_type(IdlTokenType.EQUALS):
      if self._reader.read_if_type(IdlTokenType.BEGIN_ARGS):
        # [Exposed=(Window, Worker)]
        self._check_extension_kind(matched[0], IdlExtensionKind.IDENT_LIST)
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
              matched[0], IdlExtensionKind.NAMED_ARG_LIST)
          args = self.read_arg_list()
          return matched[0](argsName=name, args=args)
        else:
          # [PutForwards=name]
          if matched[0].kind == IdlExtensionKind.IDENT_LIST:
            return matched[0](args=[name])
          else:
            self._check_extension_kind(matched[0], IdlExtensionKind.IDENT)
            return matched[0](arg=name)
    else:
      # [Replaceable]
      self._check_extension_kind(matched[0], IdlExtensionKind.NO_ARGS)
      return matched[0]()
