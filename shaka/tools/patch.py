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

"""Generates a copy of a given file with a given patch applied."""

import argparse
import os
import subprocess
import sys


def ApplyPatch(patch, directory, output):
  """Applies the given patch."""
  # patch assumes that -i is relative to -d, so make the path absolute.
  patch_abs = os.path.abspath(patch)
  proc = subprocess.Popen(
      ['patch', '-s', '-p1', '-r', '-', '-i', patch_abs, '-d', directory,
       '-o', os.path.abspath(output)],
      stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = proc.communicate()
  if proc.returncode != 0:
    print 'Standard output:'
    print stdout
    print
    print 'Standard error:'
    print stderr
    return proc.returncode

  return 0


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--patch', help='The path to the patch file')
  parser.add_argument('--dir', help='The directory that contains the sources')
  parser.add_argument('--output', help='The path to the output file')

  ns = parser.parse_args(args)
  return ApplyPatch(ns.patch, ns.dir, ns.output)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
