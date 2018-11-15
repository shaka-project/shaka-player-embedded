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


"""Defines common utilities for embedding files in the binary."""

from __future__ import division

import contextlib
import datetime
import zlib


_LICENSE = """// Copyright %s Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
"""


class CodeWriter(object):
  """A class to write generated C++ code."""

  def __init__(self, output):
    self._out = output
    self._indent = 0

    year = datetime.datetime.now().year
    output.write(_LICENSE % year)
    output.write('//\n')
    output.write('// AUTO-GENERATED: DO NOT EDIT\n\n')

  def Write(self, line='', *args, **kwargs):
    """Writes a formatted line of code with the current indentation."""
    result = line % args
    offset = self._indent * 2 + kwargs.get('offset', 0)
    indent = ' ' * offset if result else ''
    self._out.write(indent + result + '\n')

  @contextlib.contextmanager
  def Block(self, prefix, assign=False, semicolon=False):
    """Returns a context manager that creates an indented block."""
    self.Write('%s = {' if assign else '%s {', prefix)
    self._indent += 1
    yield
    self._indent -= 1
    self.Write('};' if assign or semicolon else '}')

  @contextlib.contextmanager
  def Namespace(self, name=None):
    """Returns a context manager that writes a namespace."""
    self.Write('namespace %s {', name or '')
    self.Write()
    yield
    self.Write()
    if name:
      self.Write('} // namespace %s', name)
    else:
      self.Write('} // namespace')


class CompressedCodeWriter(CodeWriter):
  """A CodeWriter that can handle compressing static data."""

  def __init__(self, output):
    super(CompressedCodeWriter, self).__init__(output)
    output.write('#include <zlib.h>\n\n')
    output.write('#include <glog/logging.h>\n\n')

  def CompressedVariable(self, var_name, text):
    """Compresses the given data and writes variables for the data.

    This writes four variables:
    |%s_data| is the byte array containing the compressed data.
    |%s_size| is the number of compressed bytes.
    |%s_uncompressed_size| is the number of uncompressed bytes.
    |%s_uncompressed| will hold the uncompressed data once uncompressed.

    Args:
      var_name: The string prefix for the variable names.
      text: A string containing the input data.
    """
    data = zlib.compress(text, 9)

    with self.Block('const uint8_t %s_data[]' % var_name, assign=True):
      for start in xrange(0, len(data), 12):
        self.Write(' '.join('0x%02x,' % ord(b) for b in data[start:start+12]))
    self.Write('constexpr const size_t %s_size = %d;', var_name, len(data))

    self.Write('constexpr const size_t %s_uncompressed_size = %d;',
               var_name, len(text))
    self.Write('uint8_t* %s_uncompressed;', var_name)
    self.Write('// Compression ratio: %.2f', len(data) / len(text))

  def Decompress(self, var_name):
    """Writes the code to decompress the given variable."""
    self.Write('%s_uncompressed = new uint8_t[%s_uncompressed_size];', var_name,
               var_name)
    self.Write('uLongf %s_temp_size = %s_uncompressed_size;', var_name,
               var_name)
    self.Write('CHECK_EQ(Z_OK, uncompress(%s_uncompressed, &%s_temp_size,',
               var_name, var_name)
    self.Write('                          %s_data, %s_size));', var_name,
               var_name)
