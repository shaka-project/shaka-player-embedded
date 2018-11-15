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


"""Generates a .cc file that embeds the V8 snapshot.

This defines the following function

void shaka::SetupV8Snapshots();
"""

import argparse
import sys

import embed_utils


def _SetupStartupData(writer, var_name):
  """Writes the code to setup a StartupData variable."""
  writer.Write(
      '%s_startup_data.data = reinterpret_cast<const char*>(%s_uncompressed);',
      var_name, var_name)
  writer.Write('%s_startup_data.raw_size = %s_uncompressed_size;', var_name,
               var_name)


def _GenerateFile(output):
  """Generates a C++ file which embeds the snapshot files."""
  with open('snapshot_blob.bin', 'rb') as f:
    snapshot_data = f.read()
  with open('natives_blob.bin', 'rb') as f:
    natives_data = f.read()

  writer = embed_utils.CompressedCodeWriter(output)
  writer.Write('#include <v8.h>')
  writer.Write()
  writer.Write('#include "src/util/utils.h"')
  writer.Write()

  with writer.Namespace('shaka'):
    with writer.Namespace():
      writer.CompressedVariable('snapshot', snapshot_data)
      writer.Write('v8::StartupData snapshot_startup_data;')
      writer.Write()
      writer.CompressedVariable('natives', natives_data)
      writer.Write('v8::StartupData natives_startup_data;')

    writer.Write()
    writer.Write('void SetupV8Snapshots();')
    writer.Write()
    with writer.Block('void SetupV8Snapshots()'):
      writer.Decompress('snapshot')
      _SetupStartupData(writer, 'snapshot')
      writer.Write('v8::V8::SetSnapshotDataBlob(&snapshot_startup_data);')
      writer.Write()
      writer.Decompress('natives')
      _SetupStartupData(writer, 'natives')
      writer.Write('v8::V8::SetNativesDataBlob(&natives_startup_data);')


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--output', dest='output',
                      help='The filename to output to.')

  ns = parser.parse_args(args)
  with open(ns.output, 'w') as output:
    _GenerateFile(output)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
