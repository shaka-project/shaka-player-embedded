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

"""Verifies changes conform to project requirements."""

import argparse
import logging
import os
import re
import subprocess
import sys

import clang_tidy
import utils

TOOLS_DIR = os.path.dirname(os.path.realpath(__file__))
ROOT_DIR = os.path.join(TOOLS_DIR, '..', '..')

_LICENSE = r"""(##?|//) Copyright 20\d\d Google LLC
\1
\1 Licensed under the Apache License, Version 2.0 \(the "License"\);
\1 you may not use this file except in compliance with the License.
\1 You may obtain a copy of the License at
\1
\1     https://www.apache.org/licenses/LICENSE-2.0
\1
\1 Unless required by applicable law or agreed to in writing, software
\1 distributed under the License is distributed on an "AS IS" BASIS,
\1 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
\1 See the License for the specific language governing permissions and
\1 limitations under the License.
"""
_STEPS = []


def _Check(func):
  """A decorator for a presubmit check."""
  _STEPS.append(func)
  return func


def _GetBinary(bin_name, parsed_args):
  """Returns the binary path for the given program."""
  attr_name = bin_name.replace('-', '_')
  if getattr(parsed_args, attr_name):
    return getattr(parsed_args, attr_name)
  elif utils.ExistsOnPath(bin_name):
    return bin_name
  elif utils.ExistsOnPath(bin_name + '.exe'):
    return 'clang-tidy.exe'
  elif parsed_args.force:
    logging.error('Unable to find %s binary on PATH', bin_name)
    return None
  else:
    logging.warn("Skipping %s test since we can't find it", bin_name)
    return None


@_Check
def _CheckLicense(_):
  logging.info('Checking license headers for files...')
  ignore_files = [
      '.clang-format',
      '.gitattributes',
      '.gitignore',
      '.gitmodules',
      'LICENSE',
      'LICENSE.chromium',
      'sample_xcode_project/LICENSE',
      'shaka/js/shaka-player.compiled.js',
      'shaka/js/shaka-player.compiled.debug.js',
  ]
  ignore_extensions = [
      '.dat',
      '.json',
      '.md',
      '.mp4',
      '.patch',
      '.pbxproj',
      '.plist',
      '.png',
      '.jpg',
      '.txt',
      '.storyboard',
      '.webm',
      '.xml',
  ]

  log = subprocess.check_output(['git', '-C', ROOT_DIR, 'ls-files'])
  has_bad_license = False
  for path in log.strip().split('\n'):
    full_path = os.path.join(ROOT_DIR, path)
    if (path.startswith('third_party') or path in ignore_files or
        os.path.splitext(path)[1] in ignore_extensions or
        not os.path.isfile(full_path)):
      continue

    with open(full_path, 'r') as f:
      text = f.read()

    match = re.match(r'#!(/usr/bin/python\d*|/bin/bash)\n', text)
    if match:
      text = text[match.end(0):]

    if not re.match(_LICENSE, text):
      if not has_bad_license:
        logging.error('File(s) have invalid license header:')
        has_bad_license = True
      logging.error('  %s', path)

  return 1 if has_bad_license else 0


@_Check
def _CheckUmbrellaHeader(_):
  logging.info('Checking umbrella header includes everything...')
  includes = {'ShakaPlayerEmbedded.h'}
  umbrella = os.path.join(
      ROOT_DIR, 'shaka', 'include', 'shaka', 'ShakaPlayerEmbedded.h')
  with open(umbrella) as f:
    for line in f:
      match = re.match(r'#\s*(?:include|import) "([^"]+)"', line)
      if match:
        includes.add(match.group(1))
      else:
        # Header should only be #include, #if, and comments.
        assert re.match(r'(//.*)?$|#\s*(end)?if', line), line

  real_files = set()
  base = os.path.join(ROOT_DIR, 'shaka', 'include', 'shaka')
  for root, _, files in os.walk(base):
    for f in files:
      real_files.add(os.path.relpath(os.path.join(root, f), base))
  # Add in generated headers.
  real_files.update(
      f for f in os.listdir(os.path.join('gen', 'shaka')) if f.endswith('.h'))

  extra_includes = includes - real_files
  if extra_includes:
    logging.error('Extra include directives:')
    for i in extra_includes:
      logging.error('  %s', i)
    return 1
  missing_includes = real_files - includes
  if missing_includes:
    logging.error('Missing include directives:')
    for i in missing_includes:
      logging.error('  %s', i)
    return 1

  return 0


@_Check
def _CheckCppLint(_):
  logging.info('Checking cpplint...')
  utils.LoadSubmodule('third_party/styleguide/src')
  files = []
  for root, _, cur_files in os.walk(os.path.join(ROOT_DIR, 'shaka', 'src')):
    files += [os.path.join(root, f) for f in cur_files
              if f.endswith('.cc') or f.endswith('.h')]

  lint = os.path.join(
      ROOT_DIR, 'third_party', 'styleguide', 'src', 'cpplint', 'cpplint.py')
  ignored_checks = [
      # cpplint doesn't calculate the correct header guard value.
      'build/header_guard',
      # We allow other C++11 headers.
      'build/c++11',
      # We use NOLINT directives that cpplint doesn't recognize.
      'readability/nolint',
      # We don't have owners on all our TODOs.
      'readability/todo',
  ]
  filter_ = '--filter=+,-' + ',-'.join(ignored_checks)
  # Filter out stdout so we only print the errors.
  with open(os.devnull, 'w') as f:
    return subprocess.call([lint, filter_] + files, stdout=f)


@_Check
def _CheckClangTidy(parsed_args):
  logging.info('Checking clang-tidy...')
  exe = _GetBinary('clang-tidy', parsed_args)
  if not exe:
    return 1 if parsed_args.force else 0

  return clang_tidy.RunClangTidy(exe, parsed_args.fix)


@_Check
def _CheckClangFormat(parsed_args):
  logging.info('Checking clang-format...')
  exe = _GetBinary('clang-format', parsed_args)
  if not exe:
    return 1 if parsed_args.force else 0

  files = []
  for d in ['demo', 'include', 'src', 'test']:
    files += utils.GetSourceFiles(os.path.join(ROOT_DIR, 'shaka', d))

  if parsed_args.fix:
    # Just run clang-format with -i to edit the files.
    return subprocess.call([exe, '-i'] + files, cwd=ROOT_DIR)

  # Run clang-format on each file and detect if it made any changes to the file.
  has_bad_format = False
  for path in files:
    result = subprocess.check_output([exe, path], cwd=ROOT_DIR)
    with open(path, 'r') as f:
      if f.read() != result:
        if not has_bad_format:
          has_bad_format = True
          logging.error('File(s) have invalid formatting:')
        logging.error('  %s', os.path.relpath(path, ROOT_DIR))

  return 1 if has_bad_format else 0


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--fix', action='store_true',
                      help='Automatically apply fixes to issues.')
  parser.add_argument('--clang-tidy',
                      help='Use the given binary for clang-tidy.')
  parser.add_argument('--clang-format',
                      help='Use the given binary for clang-format.')
  parser.add_argument('--force', action='store_true',
                      help="Fail checks if required tools can't be found")

  ns = parser.parse_args(args)
  for func in _STEPS:
    if func(ns) != 0:
      return 1

  return 0


if __name__ == '__main__':
  logging.getLogger().setLevel(logging.INFO)
  sys.exit(main(sys.argv[1:]))
