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


"""Generates a .cc file that embeds the JS tests in the binary.

This defines the following function which should be used in the tests to load
the tests:

void shaka::LoadJsTests();
"""

from __future__ import division

import argparse
import os
import sys

import embed_utils


def GetVariableName(path, variables):
  """Returns a variable name for the given file.

  This also updates the array of used variables with the new variable.
  """
  name = os.path.basename(path)[:-3]  # Get filename and strip '.js'
  assert all(n != name for _, n in variables)
  variables.append((os.path.basename(path), name))
  return name


def GenerateFile(file_paths, output):
  """Generates a C++ file which embeds the contents of the given files."""
  variables = []  # A list of (script_name, var_name)

  writer = embed_utils.CompressedCodeWriter(output)
  writer.Write('#include <vector>')
  writer.Write()
  writer.Write('#include "src/core/js_manager_impl.h"')
  writer.Write('#include "src/util/utils.h"')
  writer.Write()

  with writer.Namespace('shaka'):
    with writer.Namespace():
      for in_path in file_paths:
        with open(in_path, 'rb') as r:
          text = r.read()

        name = GetVariableName(in_path, variables)
        writer.CompressedVariable(name, text)
        writer.Write()

    writer.Write()
    writer.Write('void LoadJsTests();')
    writer.Write()
    with writer.Block('void LoadJsTests()'):
      writer.Write('auto engine = JsManagerImpl::Instance();')
      writer.Write()

      for script_name, var_name in variables:
        writer.Decompress(var_name)
        writer.Write('CHECK(engine->RunScript("%s", %s_uncompressed, '
                     '%s_uncompressed_size)->GetValue());',
                     script_name, var_name, var_name)
        writer.Write()


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--output', dest='output',
                      help='The filename to output to')
  parser.add_argument('files', nargs='+',
                      help='The input files to embed.')

  ns = parser.parse_args(args)
  with open(ns.output, 'w') as output:
    GenerateFile(ns.files, output)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
