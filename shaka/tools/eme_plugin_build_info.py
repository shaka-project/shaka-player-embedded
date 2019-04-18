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


"""Parses EME implementation files and prints build info for them.

This extracts information from the implementation files that GN needs to build
the plugins.  This includes pulling the include directory paths, dependent
libraries, and source files.  This prints the output so that GN can parse
it and use it in BUILD.gn.
"""

from __future__ import print_function

import argparse
import json
import os
import sys


def _ParsePlugin(file_path):
  """Reads the given file and parses it into an object."""
  with open(file_path, 'r') as f:
    obj = json.load(f)
    obj['path'] = file_path
    return obj


def _PrintBuildInfo(plugins, config):
  """Collects and prints the required build info from the given plugins."""
  libs = []
  sources = []
  include_dirs = []
  ldflags = []
  copy_files = []

  def Expand(path, base):
    return os.path.normpath(os.path.join(base, path % config))
  def Update(lst, path, base):
    # Append to the list if it doesn't already exist.  This supports the path
    # being a format string to create dynamic paths.
    path = Expand(path, base)
    if path not in lst:
      lst.append(path)

  for plugin in plugins:
    base = os.path.abspath(os.path.dirname(plugin['path']))
    for lib in plugin.get('libraries', []):
      path = Expand(lib, base)
      if lib.endswith('.framework'):
        ldflags += [
            '-F' + os.path.dirname(path),
            '-framework', os.path.basename(path).replace('.framework', ''),
        ]
        copy_files.append(path)
      else:
        if os.path.splitext(lib)[1] in {'.dll', '.dylib', '.so'}:
          copy_files.append(path)
        Update(libs, lib, base)

    for impl in plugin['implementations']:
      for src in impl.get('sources', []):
        Update(sources, src, base)
      for d in impl.get('include_dirs', []):
        Update(include_dirs, d, base)

  def PrintList(name, lst):
    body = ','.join('"%s"' % l for l in lst)
    print('%s = [ %s ]' % (name, body))
  PrintList('libs', libs)
  PrintList('sources', sources)
  PrintList('include_dirs', include_dirs)
  PrintList('ldflags', ldflags)
  PrintList('copy_files', copy_files)


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--config', required=True, help='The configuration name')
  parser.add_argument('--os', required=True, help='The target OS')
  parser.add_argument('--arch', required=True,
                      help='The target CPU architecture')
  parser.add_argument('files', nargs='+',
                      help='The JSON files that define the implementations')

  ns = parser.parse_args(args)
  plugins = map(_ParsePlugin, ns.files)

  # This represents the format replacements that the JSON file can include.  Any
  # path in the JSON file can include these to produce dynamic paths.
  # e.g.: "foo/%(config)s/bar" -> "foo/Debug/bar"
  config = {
      'config': ns.config,
      'arch': ns.arch,
      'os': ns.os,
  }
  _PrintBuildInfo(plugins, config)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))


