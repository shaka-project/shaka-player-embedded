#!/usr/bin/python
# Copyright 2016 Google LLC
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

"""Compiles the library, demo, and tests.
"""

from __future__ import print_function

import argparse
import os
import shutil
import subprocess
import sys

ROOT_DIR = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(ROOT_DIR, 'shaka', 'tools'))
import utils


def CopyDataFiles(config_dir, dest, link=False):
  """Copies the data files to the given destination."""
  def CopyFile(*path, **kwargs):
    abs_src = os.path.join(ROOT_DIR, *path)
    abs_dest = os.path.join(dest, kwargs.get('name', path[-1]))
    if link and hasattr(os, 'symlink'):
      try:
        os.remove(abs_dest)
      except IOError:
        pass
      os.symlink(abs_src, abs_dest)
    else:
      shutil.copy(abs_src, abs_dest)

  if utils.GetGnArg(config_dir, 'is_debug', is_string=False) == 'true':
    CopyFile('shaka', 'js', 'shaka-player.compiled.debug.js',
             name='shaka-player.compiled.js')
  else:
    CopyFile('shaka', 'js', 'shaka-player.compiled.js')


def Build(build_dir, clean=False):
  """Builds the library."""
  if clean:
    return subprocess.call(['ninja', '-C', build_dir, '-t', 'clean'])
  else:
    return subprocess.call(['ninja', '-C', build_dir, 'player'])


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--config-name', dest='config',
      help='Do a special in-tree build with this configuration name.')
  parser.add_argument('--clean', dest='clean', action='store_true',
                      help='Delete all temporary object files for this config.')
  parser.add_argument('--data-dir', dest='data_dir',
                      help='The directory to copy program data to.')
  parser.add_argument('--symlink-data', dest='symlink', action='store_true',
                      help='Don\'t copy program data, create symlinks instead.')

  ns = parser.parse_args(args)
  config_dir = utils.ConfigPath(ns.config)
  if (not os.path.exists(os.path.join(config_dir, 'build.ninja')) and
      os.path.exists(os.path.join(config_dir, 'args.gn'))):
    print('Ninja files missing, trying to recover them...', file=sys.stderr)
    configure = os.path.join(ROOT_DIR, 'configure')
    if subprocess.call([configure, '--recover', '--config-name',
                        ns.config]) != 0:
      return 1

  utils.CheckConfigName(ns.config)

  if Build(config_dir, ns.clean) != 0:
    return 1

  if not ns.clean:
    CopyDataFiles(config_dir, ns.data_dir or config_dir, ns.symlink)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
