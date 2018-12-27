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
  IDENT_LIST = 'IdentList'


class Extension(object):
  """Defines a base class for extensions.

  Derived classes are expected to define the following type fields:
  - name: A string name of the extension.
  - kinds: A list of ExtensionKind for the allowed kinds of extension.
  - locations: A list of ExtensionLocation for where the extension can be.

  The constructor will be called with name-value pairs based on the kind of
  extension this is.  If multiple kinds are allowed, the constructor is called
  with arguments based on the kind in the IDL file.
  """

  def __init__(self, **kwargs):
    for k, v in kwargs.items():
      setattr(self, k, v)


# A signal value used to indicate a default value is "null".
IdlNull = object()  # pylint: disable=invalid-name


class DebugInfo(collections.namedtuple('DebugInfo', ('lineno', 'col', 'line'))):
  """Defines where in the IDL an element was defined.

  Properties:
    lineno: The 1-based line number where the element starts.
    col: The 1-based column offset where the element starts.
    line: The string line that the definition starts at.  This includes any text
      before the definition itself.  This only includes the first line if the
      element spans multiple.
  """
  __slots__ = ()


class Argument(collections.namedtuple(
    'Argument', ('name', 'type', 'optional', 'is_variadic', 'default',
                 'extensions'))):
  """Defines an argument to a method.

  Properties:
    name: The string name of the argument.
    type: An IdlType object that describes the type of the argument.
    optional: Whether this argument is optional.
    is_variadic: Whether this argument is variadic; this will only be True for
      the last argument in the list.
    default: The default value of the argument.  This can be a number, string,
      boolean, empty list, IdlNull, or None (for no default).
    extensions: An array of extension objects attached to this argument.
  """
  __slots__ = ()


class IdlType(collections.namedtuple(
    'IdlType', ('name', 'nullable', 'element_type', 'extensions'))):
  """Defines a type of an attribute or argument.

  Properties:
    name: The string name of the type.
    nullable: Whether the type can be null.
    element_type: If present, represents the inner types of a templated type.
      For types like "sequence" this is an IdlType for the element type;
      for the "record" type, this is a tuple of (key, value) of IdlType objects.
    extensions: An array of extension objects attached to this type.
  """
  __slots__ = ()


class DictMember(collections.namedtuple(
    'DictMember', ('name', 'type', 'default', 'is_required', 'extensions',
                   'doc', 'debug', 'docDebug'))):
  """Defines a member of a Dictionary.

  Properties:
    name: The string name of the member.
    type: An IdlType object defining the type of the member.
    default: The default value of the member.  This can be a number, string,
      boolean, empty list, IdlNull, or None (for no default).
    is_required: Whether the member is required to be present.
    extensions: An array of extension objects attached to this member.
    doc: The string comment describing the member.
    debug: A DebugInfo object describing where this member starts.
    docDebug: A DebugInfo object describing where the docstring starts.  This
      is None if there is no docstring.
  """
  __slots__ = ()


class Definition(collections.namedtuple(
    'Definition', ('name', 'kind', 'members', 'base', 'is_partial',
                   'extensions', 'doc', 'debug', 'docDebug'))):
  """Defines a type definition (e.g. an 'interface').

  Properties:
    name: The string name of the type.
    kind: The kind of definition this is; will be one of: 'dictionary',
      'interface', or 'mixin'.
    members: A list of objects for the members in the type.  These will be
      different types based on what kind of member and the |kind|.  For example,
      this could be a Constant or a DictMember.
    base: A string base class name, or None for no base.
    is_partial: Whether this is a partial dictionary definition.
    extensions: A list of extension objects on this dictionary.
    doc: The string comment describing the type.
    debug: A DebugInfo object describing where this dictionary starts.
    docDebug: A DebugInfo object describing where the docstring starts.  This
      is None if there is no docstring.
  """
  __slots__ = ()


class Results(collections.namedtuple('Results', ('types',))):
  """Defines the results of parsing an IDL file.

  Properties:
    types: A list of Definition objects the file defines.
  """
  __slots__ = ()
