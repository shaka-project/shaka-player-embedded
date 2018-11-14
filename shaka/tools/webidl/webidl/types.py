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

"""Defines the AST types for IDL parsing."""

from __future__ import print_function

import collections
import sys

try:
  import enum  # pylint: disable=g-import-not-at-top
except ImportError:
  print('Requires the "enum34" pip polyfill to be installed', file=sys.stderr)
  raise


class ExtensionLocation(enum.Enum):
  """Defines possible locations an extension can be put."""
  # e.g. an 'interface' definition.
  DEFINITION = 'definition'
  # An annotated type (e.g. as an argument type).  Note this is different than
  # ARGUMENT.  If the location is ambiguous, it favors the TYPE; this means that
  # the extension will appear on the IdlType and not on the Argument.
  TYPE = 'type'
  MEMBER = 'member'
  MIXIN_MEMBER = 'mixin member'
  NAMESPACE_MEMBER = 'namespace member'
  DICTIONARY_MEMBER = 'dictionary member'
  ARGUMENT = 'argument'


class ExtensionKind(enum.Enum):
  """Defines the type of extension this is.

  The type of extension defines the fields that exist on the object.  The
  constructor is given named arguments for each of the fields.
  """
  # This has no fields.
  # For example: [Replaceable]
  NO_ARGS = 'NoArgs'
  # - 'args': A list of Argument objects for the args.
  # For example: [Constructor(double x, double y)]
  ARG_LIST = 'ArgList'
  # - 'argsName': The string name given.
  # - 'args': A list of Argument objects for the args.
  # For example: [NamedConstructor=Image(DOMString src)]
  NAMED_ARG_LIST = 'NamedArgList'
  # - 'arg': The string identifier given.
  # For example: [PutForwards=name]
  IDENT = 'Ident'
  # - 'args': A list of strings for the identifiers given.
  # For example: [Exposed=(Window,Worker)]
  # Note this allows using [Exposed=Window] for a single item.
  IDENT_LIST = 'IdentList'


class Extension(object):
  """Defines a base class for extensions.

  Derived classes are expected to define the following type fields:
  - name: A string name of the extension.
  - kind: An ExtensionKind for the kind of extension.
  - locations: A list of ExtensionLocation for where the extension can be.

  The constructor will be called with name-value pairs based on the kind of
  extension this is.
  """

  def __init__(self, **kwargs):
    for k, v in kwargs.items():
      setattr(self, k, v)


# A signal value used to indicate a default value is "null".
IdlNull = object()  # pylint: disable=invalid-name


class Argument(collections.namedtuple(
    'Argument', ('name', 'type', 'optional', 'is_variadic', 'default'))):
  """Defines an argument to a method.

  Properties:
    name: The string name of the argument.
    type: An IdlType object that describes the type of the argument.
    optional: Whether this argument is optional.
    is_variadic: Whether this argument is variadic; this will only be True for
      the last argument in the list.
    default: The default value of the argument.  This can be a number, string,
      boolean, empty list, IdlNull, or None (for no default).
  """
  __slots__ = ()


class IdlType(collections.namedtuple(
    'IdlType', ('name', 'nullable', 'element_type'))):
  """Defines a type of an attribute or argument.

  Properties:
    name: The string name of the type.
    nullable: Whether the type can be null.
    element_type: If present, it is an IdlType that specifies the element type
      for an array.  In this case, |name| is always "sequence".
  """
  __slots__ = ()


class Attribute(collections.namedtuple(
    'Attribute', ('name', 'type', 'doc'))):
  """Defines an attribute on a type.

  Properties:
    name: The string name of the attribute.
    type: An IdlType object defining the type of the attribute.
    doc: The string comment describing the attribute.
  """
  __slots__ = ()


class Dictionary(collections.namedtuple(
    'Dictionary', ('name', 'attributes', 'doc'))):
  """Defines a dictionary definition.

  Properties:
    name: The string name of the dictionary.
    attributes: A list of Attribute objects for the attributes in the dict.
    doc: The string comment describing the type.
  """
  __slots__ = ()


class Results(collections.namedtuple('Results', ('types',))):
  """Defines the results of parsing an IDL file.

  Properties:
    types: A list of types the file defines.  Will always be a Dictionary type.
  """
  __slots__ = ()
