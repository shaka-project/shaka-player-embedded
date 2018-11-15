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


"""A GN utility that touches the given files."""

import argparse
import os
import sys


def Touch(path, create=True):
  if not os.path.exists(path):
    if create:
      with open(path, 'w'):
        pass
    else:
      raise IOError('File not found')
  os.utime(path, None)


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--new-file', action='append',
                      help="Touch these files and create if they don't exist.")
  parser.add_argument('files', nargs='*',
                      help='The files to update, must already exist.')

  args = parser.parse_args(sys.argv[1:])
  for f in args.new_file:
    Touch(f, create=True)
  for f in args.files:
    Touch(f, create=False)
