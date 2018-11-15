#!/usr/bin/python
# Copyright 2017 Google LLC
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

"""Parses the given Makefiles and prints the files to compile."""

import argparse
import os
import re
import sys

TOOLS_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(
    0, os.path.join(TOOLS_DIR, '..', '..', 'third_party', 'pymake', 'src'))
import pymake.data
import pymake.parser
import pymake.parserdata


def _Split(s):
  """Splits a string on white space.

  This is used to split similar to argument lists, which is used for Make
  variables.  However, this doesn't support using quoted arguments.
  """
  assert '"' not in s
  if not s.strip():
    return []
  return re.split(' +', s.strip())


class Makefile(object):
  """Defines the parsed results of one or more Makefiles."""

  def __init__(self, paths):
    """Parses the given Makefile paths into a new object."""
    self._makefile = pymake.data.Makefile(env={}, justprint=True)
    self._dependencies = {}

    for f in paths:
      for stmt in pymake.parser.parsefile(f):
        self._ExecStatement(stmt)

  def GetListVariable(self, name):
    """Returns a list of strings from the given variable."""
    var = self._makefile.variables.get(name, expand=True)[2]
    if not var:
      return []
    return var.resolvesplit(self._makefile, self._makefile.variables)

  def GetTargetDepdency(self, target):
    """Returns a string containing the target's dependency line."""
    return self._dependencies[target].strip()

  def _ExecStatement(self, stmt):
    """Executes the given Makefile statement."""
    assert isinstance(stmt, pymake.parserdata.Statement)
    if isinstance(stmt, pymake.parserdata.Rule):
      name = stmt.targetexp.resolvestr(self._makefile, self._makefile.variables)
      value = stmt.depexp.resolvestr(self._makefile, self._makefile.variables)
      self._dependencies[name] = value
    elif (isinstance(stmt, pymake.parserdata.StaticPatternRule) or
          isinstance(stmt, pymake.parserdata.Command) or
          isinstance(stmt, pymake.parserdata.EmptyDirective)):
      pass  # Ignore commands
    elif isinstance(stmt, pymake.parserdata.Include):
      pass  # Ignore includes
    elif isinstance(stmt, pymake.parserdata.SetVariable):
      stmt.execute(self._makefile, None)
    elif isinstance(stmt, pymake.parserdata.ConditionBlock):
      for cond, children in stmt:
        if cond.evaluate(self._makefile):
          for s in children:
            self._ExecStatement(s)
          break
    else:
      assert False, 'Unknown type of statement %s' % stmt


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--prefix', help='The prefix to append to file paths')
  parser.add_argument(
      '--var-name', action='append', required=True,
      help='The name of the variable to get.  Argument can appear multiple '
           'times.')
  parser.add_argument('files', nargs='+', help='The files to parse')

  parsed_args = parser.parse_args(argv)
  makefile = Makefile(parsed_args.files)
  for name in parsed_args.var_name:
    for item in makefile.GetListVariable(name):
      if parsed_args.prefix:
        print os.path.join(parsed_args.prefix, item)
      else:
        print item


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
