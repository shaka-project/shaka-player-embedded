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

"""Generates the version.h file that contains the library version."""

from __future__ import print_function

import argparse
import os
import re
import subprocess
import sys

import embed_utils


_MAX_VERSION = 2**16 - 1
# The commits, beta version, and is-dirty (1 bit) are stored in a 16-bit block.
_MAX_COMMITS = 2**12 - 1
_MAX_BETA_VERSION = 2**3 - 1


def _GetVersion():
  """Returns the version string of the library."""
  try:
    cur_dir = os.path.dirname(__file__)
    cmd = ['git', '-C', cur_dir, 'describe', '--tags', '--dirty']
    with open(os.devnull, 'w') as null:
      return subprocess.check_output(cmd, stderr=null).strip()
  except subprocess.CalledProcessError:
    # TODO: Remove once the first version is tagged.
    return 'v0.1.0-beta'


def ParseVersion(version_str):
  """Parses the given version string.

  The version numbering is such that each commit gets a unique version number
  and can be compared using integer comparisons.  The major, minor, and revision
  components are just 16-bit portions of the number; the tag forms the low
  16-bits.

  The tag contains the beta version number, the commit count since the last
  version, and whether the working directory is dirty.  If this is a beta
  version, (e.g. v1.2.3-beta), then the returned number will actually be the
  previous version (1, 2, 2) and the tag will contain the beta version number.
  Because the beta version is the highest in the bit ordering, it will make it
  larger than a version string from the previous version (v1.2.2).

  64               48               32               16  13           0
  |-------------------------------------------------------------------|
  |      Major     |      Minor     |     Revision   |       Tag      |
  |                |                |                | B |  Commits |D|
  |-------------------------------------------------------------------|

  Args:
    version: The version string to parse.

  Returns:
    The parsed version, as a 4-tuple of numbers.
  """
  match = re.match(
      r'^v(\d+)\.(\d+)\.(\d+)(-beta(\d*))?(-(\d+)-g[0-9a-f]+)?(-dirty)?$',
      version_str)
  if not match:
    raise ValueError('Invalid version format: ' + version_str)
  major = int(match.group(1))
  minor = int(match.group(2))
  revision = int(match.group(3))
  tag = 0
  if any(v > _MAX_VERSION for v in (major, minor, revision)):
    raise ValueError('Version number too large: ' + version_str)
  if major == 0 and minor == 0 and revision == 0:
    raise ValueError('Version cannot be v0.0.0')

  if match.group(4):
    # If this is a pre-release version, reduce the numbers so the release
    # version will be larger than this number.
    beta_version = int(match.group(5) or 1)
    if beta_version == 0 or beta_version > _MAX_BETA_VERSION:
      raise ValueError('Invalid beta version number: ' + version_str)
    tag = beta_version << 13

    if revision != 0:
      revision -= 1
    elif minor != 0:
      revision = _MAX_VERSION
      minor -= 1
    else:
      assert major > 0
      minor = revision = _MAX_VERSION
      major -= 1

  if match.group(6):
    commits = int(match.group(7))
    if commits == 0:
      raise ValueError('Invalid version format: ' + version_str)
    if commits > _MAX_COMMITS:
      raise ValueError('Too many commits since last release: ' + version_str)
    tag |= commits << 1

  if match.group(8):
    tag |= 1

  return major, minor, revision, tag


def _GenVersion(output):
  """Writes the version.h file to the given file object."""
  version = _GetVersion()
  major, minor, revision, tag = ParseVersion(version)

  assert major >= 0 and major <= _MAX_VERSION
  assert minor >= 0 and minor <= _MAX_VERSION
  assert revision >= 0 and revision <= _MAX_VERSION
  assert tag >= 0 and tag <= _MAX_VERSION
  writer = embed_utils.CodeWriter(output)
  writer.Write('#define SHAKA_VERSION_MAJOR %dULL' % major)
  writer.Write('#define SHAKA_VERSION_MINOR %dULL' % minor)
  writer.Write('#define SHAKA_VERSION_REVISION %dULL' % revision)
  writer.Write('#define SHAKA_VERSION_TAG %dULL' % tag)
  writer.Write('#define SHAKA_VERSION_INT ('
               '(SHAKA_VERSION_MAJOR << 48ULL) | '
               '(SHAKA_VERSION_MINOR << 32ULL) | '
               '(SHAKA_VERSION_REVISION << 16ULL) | (SHAKA_VERSION_TAG))')
  writer.Write('#define SHAKA_VERSION_STR "%s"' % version)
  writer.Write()

  writer.Write('#ifdef __cplusplus')
  writer.Write('extern "C" {')
  writer.Write('#endif  // __cplusplus')

  writer.Write('/** @return The runtime version of the library. */')
  writer.Write('const char* GetShakaEmbeddedVersion(void);')

  writer.Write('#ifdef __cplusplus')
  writer.Write('}')
  writer.Write('#endif  // __cplusplus')


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--output', required=True,
                      help='The path to the file to generate.')

  parsed_args = parser.parse_args(argv)

  output_dir = os.path.dirname(parsed_args.output)
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)
  with open(parsed_args.output, 'w') as f:
    _GenVersion(f)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
