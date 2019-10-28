#!/usr/bin/python
# Copyright 2019 Google LLC
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

"""Generates the Info.plist file for the frameworks."""

import argparse
import os
import sys

import version

def _GenInfoPlist(output):
  """Writes the Info.plist file to the given file object."""
  version_str = version.GetVersionStr()
  major, minor, revision, tag = version.ParseVersion(version_str)

  body = """
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleVersion</key>
  <string>%s</string>
  <key>CFBundleShortVersionString</key>
  <string>%s</string>
  <key>CFBundleSignature</key>
  <string>????</string>
  <key>CFBundlePackageType</key>
  <string>FMWK</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundleIdentifier</key>
  <string>${IOS_BUNDLE_ID_PREFIX}.${EXECUTABLE_NAME:rfc1034identifier}</string>
  <key>CFBundleExecutable</key>
  <string>${EXECUTABLE_NAME}</string>
  <key>CFBundleDevelopmentRegion</key>
  <string>English</string>
</dict>
</plist>
  """

  # Note that only the first three values are used by the OS, the tag is
  # ignored.
  version_out = '%s.%s.%s.%s' % (major, minor, revision, tag)
  output.write(body % (version_out, version_out))


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--output', required=True,
                      help='The path to the file to generate.')

  parsed_args = parser.parse_args(argv)

  output_dir = os.path.dirname(parsed_args.output)
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)
  with open(parsed_args.output, 'w') as f:
    _GenInfoPlist(f)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
