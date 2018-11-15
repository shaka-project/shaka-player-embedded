#!/usr/bin/python
# Copyright 2015 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Modified from:
# https://github.com/google/shaka-packager/blob/master/packager/third_party/curl/config/mac/find_curl_ca_bundle.sh

from __future__ import print_function

import os


paths = [
    '/opt/local/etc/openssl/cert.pem',  # macports
    '/opt/local/share/curl/curl-ca-bundle.crt', # macports
    '/usr/local/etc/openssl/cert.pem',  # homebrew
]


def find_ca():
  """Returns the path to an existing CA bundle."""
  for f in paths:
    if os.path.isfile(f):
      return f
  raise Exception('Unable to locate SSL CA cert')


if __name__ == '__main__':
  print(find_ca())
