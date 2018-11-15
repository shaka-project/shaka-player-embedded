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

"""Runs clang-tidy over the code to check for errors.

This requires clang-tidy to be installed on PATH.
"""

from __future__ import print_function

import argparse
import json
import os
import subprocess
import sys

import utils

TOOLS_DIR = os.path.dirname(os.path.realpath(__file__))
ROOT_DIR = os.path.join(TOOLS_DIR, '..', '..')
sys.path.append(os.path.join(ROOT_DIR, 'tools', 'clang', 'pylib', 'clang'))
import compile_db

DISABLED_CHECKS = [
    # We disabled exceptions.
    'cert-err58-cpp',
    'hicpp-noexcept-move',
    'misc-noexcept-move-constructor',
    'performance-noexcept-move-constructor',
    # We have some extra flags clang-tidy may not recognize.
    'clang-diagnostic-unused-command-line-argument',
    # We are safe with new/delete; plus we have a custom 'new' which makes
    # ownership hard to determine.
    'cppcoreguidelines-owning-memory',
    # We are safe with uninitialized structs.
    'cppcoreguidelines-pro-type-member-init',
    'hicpp-member-init',
    # We need to do reinterpret_cast for low-level interactions.
    'cppcoreguidelines-pro-type-reinterpret-cast',
    # We use util::StringPrintf, which we can't whitelist.
    'cppcoreguidelines-pro-type-vararg',
    'hicpp-vararg',
    # We are safe with pointer arithmetic.
    'cppcoreguidelines-pro-bounds-array-to-pointer-decay',
    'cppcoreguidelines-pro-bounds-constant-array-index',
    'cppcoreguidelines-pro-bounds-pointer-arithmetic',
    'hicpp-no-array-decay',
    # We can't use dynamic_cast<T> since we don't have RTTI.
    'cppcoreguidelines-pro-type-static-cast-downcast',
    # We allow default arguments.
    'fuchsia-default-arguments',
    # We allow operator overloading.
    'fuchsia-overloaded-operator',
    # We are safe with signed bitwise math; plus there is a lot of third-party
    # macros that use it.
    'hicpp-signed-bitwise',
    # This suggests =default for constructors too, which seems weird.
    'hicpp-use-equals-default',
    'modernize-use-equals-default',
    # This doesn't allow single statement bodies.
    'google-readability-braces-around-statements',
    'hicpp-braces-around-statements',
    'readability-braces-around-statements',
    # Allow missing owner of TODO.
    'google-readability-todo',
    # We can't use make_unique since it is C++14.
    'modernize-make-unique',
    # Google style is pass-by reference.
    'modernize-pass-by-value',
    # We use value parameters for "simple" types like RefPtr<T>.
    'performance-unnecessary-value-param',
    # This seems to says the static field definitions are redundant, which they
    # aren't.
    'readability-redundant-declaration',

    # TODO: Consider enabling this.  It doesn't allow pointers in conditionals.
    # Even with options, it still won't allow (ptr && ptr->foo()).
    'readability-implicit-bool-cast',
    'readability-implicit-bool-conversion',
    # TODO: Consider enabling this.
    'modernize-use-default-member-init',
    # TODO: This gives a number of false-positives when the upcast is useful.
    'misc-misplaced-widening-cast',
    # TODO: This causes several false-positives.
    'clang-analyzer-core.UndefinedBinaryOperatorResult',
    # TODO: This causes errors from std::shared_ptr<T>.
    'clang-analyzer-core.uninitialized.UndefReturn',
    # TODO: This causes errors from std::mutex.
    'clang-analyzer-core.StackAddressEscape',
]
CHECKS = '*,-' + ',-'.join(DISABLED_CHECKS)


def _GenerateDatabase(build_dir):
  """Generates the compile_commands.json database file."""
  # Patch GetNinjaPath since we assume it is globally installed.
  compile_db.GetNinjaPath = lambda: 'ninja'

  db = json.dumps(compile_db.GenerateWithNinja(build_dir))
  db = (db.replace('-Wno-enum-compare-switch', '')
        .replace('-Wno-unused-lambda-capture', '')
        .replace('-Wno-ignored-pragma-optimize', '')
        .replace('-Wno-null-pointer-arithmetic', '')
        .replace('-Wno-return-std-move-in-c++11', ''))
  with open(os.path.join(build_dir, 'compile_commands.json'), 'w') as f:
    f.write(db)


def RunClangTidy(build_dir, clang_tidy, fix):
  """Runs clang-tidy for the given build directory."""
  _GenerateDatabase(build_dir)

  files = utils.GetSourceFiles(os.path.join(ROOT_DIR, 'shaka', 'src'))
  files = [os.path.relpath(f, build_dir) for f in files]
  cmd = [clang_tidy, '-checks=' + CHECKS, '-warnings-as-errors=*', '-p', '.']
  if fix:
    cmd += ['-fix', '-fix-errors']
  return subprocess.call(cmd + files, cwd=build_dir)


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--config-name', dest='config',
      help='Do a special in-tree build with this configuration name.')
  parser.add_argument('--fix', action='store_true',
                      help='Automatically apply fixes to issues.')
  parser.add_argument('--clang-tidy', dest='clang_tidy',
                      help='The path to a clang-tidy executable to use.')

  parsed_args = parser.parse_args(args)
  utils.CheckConfigName(parsed_args.config)

  build_dir = utils.ConfigPath(parsed_args.config)
  return RunClangTidy(build_dir, parsed_args.clang_tidy or 'clang-tidy',
                      parsed_args.fix)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
