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


"""Prepares a licenses.txt file for the package.

The license paths file is expected to contain a series of titles and paths to
output. The paths should be relative to the file itself. For example,
Title: path/to/LICENSE

The extra licenses file is expected to contain input similar to the output;
a series of licenses, each with a title on a separate line prefixed by '@'.

This file is used in the demo to display the various licenses for the
dependencies of the project.
"""

import argparse
import os
import sys
import json


def _GenLicensesFile(out, paths, extras, base_path):
  """Reads the input files, and writes a licenses.txt file to the given output.

  Args:
    out: A file object for the output.
    paths: A file object for the paths file.
    extras: A file object for the extra licenses file.
    base_path: The URL base used to resolve the relative URLs in the paths file.
  """
  licenses = []
  for line in paths:
    name, path = line.split(': ', 1)
    path = os.path.join(base_path, path.rstrip('\n'))
    with open(path, 'r') as file:
      licenses.append({'name': name, 'text': file.read()})
  while True:
    name = extras.readline()
    if not name: break
    text = extras.readline().replace('\\n', '\n')
    licenses.append({'name': name, 'text': text})
  out.write(json.dumps(licenses))


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--paths-file', required=True,
                      help='A file that contains paths to licenses.')
  parser.add_argument('--extras-file', required=True,
                      help='A file that contains extra license text, ' +
                           'copied verbatim.')
  parser.add_argument('--output', required=True,
                      help='The path to the file to generate.')

  parsed_args = parser.parse_args(argv)

  output_dir = os.path.dirname(parsed_args.output)
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)
  with open(parsed_args.output, 'w') as out:
    with open(parsed_args.paths_file, 'r') as paths:
      with open(parsed_args.extras_file, 'r') as extras:
        base_path = os.path.dirname(parsed_args.paths_file)
        _GenLicensesFile(out, paths, extras, base_path);

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
