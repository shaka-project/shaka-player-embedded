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


"""Generates a .cc file that registers the default EME implementations.

This defines the following function:

void shaka::RegisterDefaultKeySystems();
"""

from __future__ import print_function

import argparse
import json
import os
import sys

import embed_utils


def _GetHeaders(plugins):
  """Returns a set of headers from the given plugins."""
  headers = set()
  for plugin in plugins:
    headers.update(i['header'] for i in plugin['implementations'])
  return headers


def _ParsePlugin(file_path):
  """Reads the given file and parses it into an object."""
  with open(file_path, 'r') as f:
    return json.load(f)


def GenerateFile(plugins, output):
  """Generates a C++ file which registers the given implementations."""
  writer = embed_utils.CodeWriter(output)
  writer.Write('#include <atomic>')
  writer.Write()
  writer.Write('#include "shaka/eme/implementation_registry.h"')
  writer.Write()

  for header in sorted(_GetHeaders(plugins)):
    writer.Write('#include "%s"', header)
  writer.Write()

  with writer.Namespace('shaka'):
    writer.Write('void RegisterDefaultKeySystems();')
    writer.Write()
    with writer.Block('void RegisterDefaultKeySystems()'):
      # This ensures the key systems are registered exactly once, even if this
      # is called from multiple threads.  The compare_exchange_strong will
      # atomically check if it is false and replace with true on only one
      # thread.
      writer.Write('static std::atomic<bool> called{false};')
      writer.Write('bool expected = false;')
      with writer.Block('if (called.compare_exchange_strong(expected, true))'):
        for plugin in plugins:
          for impl in plugin['implementations']:
            writer.Write('eme::ImplementationRegistry::AddImplementation(')
            writer.Write('    "%s",', impl['key_system'])
            writer.Write(
                '    std::shared_ptr<eme::ImplementationFactory>{new %s});',
                impl['factory_type'])


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--output', dest='output',
                      help='The filename to output to')
  parser.add_argument('files', nargs='+',
                      help='The JSON files that define the implementations')

  ns = parser.parse_args(args)
  plugins = map(_ParsePlugin, ns.files)

  with open(ns.output, 'w') as output:
    GenerateFile(plugins, output)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

