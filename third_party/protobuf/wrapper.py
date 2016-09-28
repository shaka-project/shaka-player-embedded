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

"""A wrapper script to execute the given command.

This will simply execute the command that is specified in the command-line
arguments.  This is required since GN only accepts python files in actions.
This is used to invoke the protoc compiler.
"""

import subprocess
import sys


if __name__ == '__main__':
  subprocess.check_call(sys.argv[1:])
