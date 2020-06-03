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

"""Runs tests to verify we don't break ABI between versions.

This runs a build for the current change and checks out a given commit and
compares the two to see if there are reverse-incompatible ABI changes.  This
performs a git checkout to perform the test, but the previous state is restored
at the end of the test.
"""

from __future__ import print_function

import argparse
import logging
import os
import shutil
import subprocess
import sys
import tempfile

TOOLS_DIR = os.path.dirname(os.path.realpath(__file__))
ROOT_DIR = os.path.join(TOOLS_DIR, '..', '..')


def _GetCheckout():
  """Gets the current git branch, or the current commit if detached."""
  branch = subprocess.check_output(['git', '-C', ROOT_DIR, 'rev-parse',
                                    '--abbrev-ref', '--verify', 'HEAD']).strip()
  if branch != 'HEAD':
    return branch
  return subprocess.check_output(
      ['git', '-C', ROOT_DIR, 'rev-parse', 'HEAD']).strip()


def _IsGitDirty():
  """Returns whether the working directory is dirty."""
  return subprocess.call(['git', '-C', ROOT_DIR, 'diff', '--exit-code']) != 0


def _DoBuild(build_dir, parsed_args):
  """Configures and builds the project."""
  logging.info('Creating release build at %s...',
               subprocess.check_output(
                   ['git', '-C', ROOT_DIR, 'rev-parse', 'HEAD'])[:6])

  args = ['--release', '--no-makefile']
  if parsed_args.ccache:
    args += ['--ccache-if-possible']
  if subprocess.call([os.path.join(ROOT_DIR, 'configure')] + args,
                     cwd=build_dir) != 0:
    return 1
  return subprocess.call([os.path.join(ROOT_DIR, 'build.py')], cwd=build_dir)


def _GetSymbols(build_dir):
  """Returns a set of public symbols from the library."""
  lib_path = os.path.join(build_dir, 'libshaka-player-embedded.so')
  log = subprocess.check_output(
      ['nm', '-Dg', '--defined-only', '--format=posix', lib_path])

  symbols = set()
  for line in log.strip().split('\n'):
    symbol, type_, _, _ = line.split(' ')
    # Ignore weak symbols as they are defined outside the library.
    if type_ not in {'W', 'w', 'V', 'v', 'U'}:
      symbols.add(symbol)

  return symbols


def RunAbiTest(build_dir, parsed_args):
  """Runs the ABI tests."""
  if _DoBuild(build_dir, parsed_args) != 0:
    return 1
  symbols = _GetSymbols(build_dir)

  # Checkout the other reference and do another build.
  logging.info('Checking out %s to compare against...', parsed_args.ref)
  if subprocess.call(['git', '-C', ROOT_DIR, 'checkout', parsed_args.ref]) != 0:
    return 1
  if _DoBuild(build_dir, parsed_args) != 0:
    logging.warning("Skipping ABI test since parent commit doesn't compile")
    return 0
  old_symbols = _GetSymbols(build_dir)

  if not old_symbols.issubset(symbols):
    logging.error('Change breaks ABI compatibility.')
    logging.error('The following symbols are missing:')
    for s in old_symbols - symbols:
      logging.error('  %s', s)
    return 1

  logging.info('ABI check passed')
  return 0


def main(args):
  parser = argparse.ArgumentParser(
      description=__doc__, usage='%(prog)s [options]',
      formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('--ref', default='HEAD^',
                      help='Compare HEAD against the given ref.')
  parser.add_argument(
      '--ccache-if-possible', dest='ccache', action='store_true',
      help='Use ccache to speed up builds if found on PATH.')
  parser.add_argument(
      '--ios', action='store_true',
      help='Runs the ABI test for the iOS framework (only works on Mac).')

  parsed_args = parser.parse_args(args)
  if parsed_args.ios:
    parser.error('--ios is currently unsupported')

  if _IsGitDirty():
    logging.error(
        'Git repo is dirty; can only run tests on clean working directory')
    return 1

  prev_ref = _GetCheckout()
  temp_dir = tempfile.mkdtemp()
  try:
    return RunAbiTest(temp_dir, parsed_args)
  finally:
    shutil.rmtree(temp_dir, ignore_errors=True)
    logging.info('Restoring previous git checkout')
    subprocess.check_call(['git', '-C', ROOT_DIR, 'checkout', '-f', prev_ref])


if __name__ == '__main__':
  logging.getLogger().setLevel(logging.INFO)
  sys.exit(main(sys.argv[1:]))
