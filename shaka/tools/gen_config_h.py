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


"""Generates the shaka config.h file."""

import argparse
import os
import sys


def _GenConfig(parsed_args, out):
  print >>out, '// Copyright 2018 Google Inc.  All rights reserved.'
  if parsed_args.sdl_utils:
    print >>out, '#define SHAKA_SDL_UTILS 1'
  else:
    print >>out, '#undef SHAKA_SDL_UTILS'


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--output', required=True,
                      help='The path to the file to generate.')
  parser.add_argument('--sdl-utils', action='store_true',
                      help='Include SDL utils in the library.')

  parsed_args = parser.parse_args(argv)

  output_dir = os.path.dirname(parsed_args.output)
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)
  with open(parsed_args.output, 'w') as f:
    _GenConfig(parsed_args, f)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
