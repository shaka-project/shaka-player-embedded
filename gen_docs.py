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

"""Generates HTML documentation for the project."""

import argparse
import os
import subprocess
import sys

ROOT_DIR = os.path.dirname(os.path.realpath(__file__))
BUILD_PATH = os.path.join(ROOT_DIR, 'third_party', 'doxygen', 'build')
BIN_PATH = os.path.join(BUILD_PATH, 'bin', 'doxygen')
sys.path.append(os.path.join(ROOT_DIR, 'shaka', 'tools'))
import utils


def CompileDoxygen():
  """Compiles the doxygen program, if needed."""
  if os.path.exists(BIN_PATH):
    return 0

  if not os.path.exists(BUILD_PATH):
    os.mkdir(BUILD_PATH)

  if subprocess.call(['cmake', '../src'], cwd=BUILD_PATH) != 0:
    return 1
  return subprocess.call(['make', '-j4'], cwd=BUILD_PATH)


def GenDocs():
  """Generates the library documentation."""
  utils.LoadSubmodule('third_party/doxygen/src')
  if CompileDoxygen() != 0:
    return 1
  return subprocess.call([BIN_PATH], cwd=ROOT_DIR)


def GenerateDoxyfile(extra_path):
  """Generates the Doxyfile configuration file."""
  with open(os.path.join(ROOT_DIR, 'Doxyfile.in'), 'r') as in_file:
    with open(os.path.join(ROOT_DIR, 'Doxyfile'), 'w') as out_file:
      out_file.write(in_file.read().replace('{{EXTRA_PATH}}', extra_path))


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)

  parser.parse_args(args)

  GenerateDoxyfile(os.path.join(os.getcwd(), 'gen', 'shaka'))
  return GenDocs()


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
