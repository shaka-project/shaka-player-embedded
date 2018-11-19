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

import unittest

from webidl import parser
from webidl import types


class NoArgsExtension(types.Extension):
  name = 'NoArgs'
  kinds = [types.ExtensionKind.NO_ARGS]
  # Allow this everywhere.
  locations = list(types.ExtensionLocation)


class ArgListExtension(types.Extension):
  name = 'ArgList'
  kinds = [types.ExtensionKind.ARG_LIST]
  # Allow this everywhere.
  locations = list(types.ExtensionLocation)


class NamedArgListExtension(types.Extension):
  name = 'NamedArgList'
  kinds = [types.ExtensionKind.NAMED_ARG_LIST]
  # Allow this everywhere.
  locations = list(types.ExtensionLocation)


class IdentExtension(types.Extension):
  name = 'Ident'
  kinds = [types.ExtensionKind.IDENT]
  # Allow this everywhere.
  locations = list(types.ExtensionLocation)


class IdentListExtension(types.Extension):
  name = 'IdentList'
  kinds = [types.ExtensionKind.IDENT_LIST]
  # Allow this everywhere.
  locations = list(types.ExtensionLocation)


class TypeExtension(types.Extension):
  name = 'Type'
  kinds = [types.ExtensionKind.NO_ARGS]
  locations = [types.ExtensionLocation.TYPE]


class NonTypeExtension(types.Extension):
  name = 'NonType'
  kinds = [types.ExtensionKind.NO_ARGS]
  # Allow this everywhere except TYPE.
  locations = set(types.ExtensionLocation) - {types.ExtensionLocation.TYPE}


class DefinitionExtension(types.Extension):
  name = 'Definition'
  kinds = [types.ExtensionKind.NO_ARGS]
  locations = [types.ExtensionLocation.DEFINITION]


class MemberExtension(types.Extension):
  name = 'Member'
  kinds = [types.ExtensionKind.NO_ARGS]
  locations = [
      types.ExtensionLocation.MEMBER,
      types.ExtensionLocation.MIXIN_MEMBER,
      types.ExtensionLocation.NAMESPACE_MEMBER,
      types.ExtensionLocation.DICTIONARY_MEMBER,
  ]


ALL_EXTENSIONS = [
    NoArgsExtension, ArgListExtension, NamedArgListExtension, IdentExtension,
    IdentListExtension, TypeExtension, NonTypeExtension, DefinitionExtension,
    MemberExtension,
]


class TestBase(unittest.TestCase):

  def __init__(self, *args, **kwargs):
    super(TestBase, self).__init__(*args, **kwargs)
    self.parser = parser.IdlParser(extensions=ALL_EXTENSIONS)
