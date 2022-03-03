#!/usr/bin/python
# Copyright 2015 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Modified from:
# https://github.com/shaka-project/shaka-packager/blob/main/packager/third_party/curl/config/linux/find_curl_ca_bundle.sh

from __future__ import print_function

import os


paths = [
    '/etc/pki/tls/certs/ca-bundle.crt',
    '/etc/ssl/cert.pem',
    '/etc/ssl/certs/ca-bundle.crt',
    '/etc/ssl/certs/ca-certificates.crt',
    '/usr/local/share/certs/ca-root.crt',
    '/usr/share/ssl/certs/ca-bundle.crt',
]


def find_ca():
  """Returns the path to an existing CA bundle."""
  for f in paths:
    if os.path.isfile(f):
      return f
  raise Exception('Unable to locate SSL CA cert')


if __name__ == '__main__':
  print(find_ca())
