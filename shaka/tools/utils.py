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

"""A number of helper utilities for the scripts."""

from __future__ import print_function

import os
import platform
import re
import subprocess
import sys


ROOT_DIR = os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', '..')


def LoadSubmodule(path):
  """Downloads the submodule at the given path."""
  # If the directory already has a clone (e.g. someone ran git-clone there),
  # then absorb the .git directory so we can use it.
  subprocess.check_output(
      ['git', '-C', ROOT_DIR, 'submodule', 'absorbgitdirs', path])
  # Then update the module.
  subprocess.check_output(
      ['git', '-C', ROOT_DIR, 'submodule', 'update', '--init', '--force', path])


def GetSourceFiles(path):
  """Returns a list of all the source files that are at the given path."""
  extensions = ['.cc', '.m', '.mm']
  files = []
  for root, _, cur_files in os.walk(path):
    for f in cur_files:
      if os.path.splitext(f)[1] in extensions:
        files.append(os.path.join(root, f))
  return files


def SymLink(source, dest):
  """Creates a symlink at |dest| pointing at |source|."""
  if not os.path.exists(dest):
    # If the link is invalid, the link will exist, but it does not |exists()|,
    # so delete the original to try to fix it.
    if os.path.islink(dest):
      os.remove(dest)
    # TODO: This only works on Unix platforms.  Add support for Windows.
    os.symlink(source, dest)


def ConfigPath(config):
  """Returns the path to the given config name."""
  return os.path.join(ROOT_DIR, 'out', config) if config else '.'


def ExistsOnPath(cmd):
  """Returns whether the given executable exists on PATH."""
  paths = os.getenv('PATH').split(os.pathsep)
  return any(os.path.exists(os.path.join(d, cmd)) for d in paths)


def FindGn():
  """Returns the full path to the gn executable for this platform."""
  platform_name = platform.uname()[0]
  if platform_name == 'Linux':
    return os.path.join(ROOT_DIR, 'buildtools', 'linux64', 'gn')
  elif platform_name == 'Darwin':
    return os.path.join(ROOT_DIR, 'buildtools', 'mac', 'gn')
  elif platform_name == 'Windows' or 'CYGWIN' in platform_name:
    return os.path.join(ROOT_DIR, 'buildtools', 'win', 'gn.exe')
  else:
    raise Exception('Unknown platform name ' + platform_name)


def GetGnArg(var_name, is_string=True):
  """Gets the value of a GN build argument."""
  val = subprocess.check_output([FindGn(), '--root=' + ROOT_DIR, 'args',
                                 '.', '--list=' + var_name, '--short'])
  # Examine the output line by line, since it might contain lines that are
  # warnings.
  for line in val.splitlines():
    fmt = r'^%s = "(.*)"$' if is_string else r'^%s = (.*)$'
    match = re.match(fmt % var_name, line)
    if match:
      return match.group(1)
  return None


def GetGnVar(var_name):
  """Gets the value of a GN variable."""
  val = subprocess.check_output([FindGn(), '--root=' + ROOT_DIR, 'desc',
                                 '.', '//:get_' + var_name,
                                 'include_dirs']).strip()

  # Look for after the signal, "//value:/\n", to skip past any warnings.
  after = val.partition('//value:/\n')[2]
  if after == '//EMPTY/':
    return ''
  elif after and after[:2] == '//':
    return ROOT_DIR + after[1:]
  else:
    return after
