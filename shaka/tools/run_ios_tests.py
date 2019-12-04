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

"""Runs the unit tests on an iOS simulator."""

from __future__ import print_function

import argparse
import contextlib
import json
import os
import subprocess
import sys


_APP_NAME = 'org.chromium.gtest.tests'
_IOS_SIM_PREFIX = 'com.apple.CoreSimulator.SimRuntime.iOS-'
_SIMULATOR_PATH = ('/Applications/Xcode.app/Contents/Developer/Applications/'
                   'Simulator.app/Contents/MacOS/Simulator')
_PASS_SIGNAL_LINE = 'TEST RESULTS: PASS'
_FAIL_SIGNAL_LINE = 'TEST RESULTS: FAIL'
_MIN_VERSION = (10,)

_ROOT_DIR = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                         '..', '..')


@contextlib.contextmanager
def _TerminateProcess(proc):
  """A context manager that terminates the given process at the end."""
  yield proc
  proc.kill()


def _FindIosSimulator():
  """Returns a tuple of a UUID and whether it is booted."""
  try:
    output = subprocess.check_output(['xcrun', 'simctl', 'list', '-j',
                                      'devices'])
  except OSError:
    print('Error running "xcrun", do you have XCode installed?  Try running:',
          file=sys.stderr)
    print('  xcode-select --install', file=sys.stderr)
    raise

  obj = json.loads(output)
  largest_version = None
  largest_uuid = None
  for key, value in obj['devices'].iteritems():
    if key.startswith(_IOS_SIM_PREFIX):
      version = key[len(_IOS_SIM_PREFIX):].replace('-', '.')
    elif key.startswith('iOS '):
      version = key[4:]
    else:
      continue

    version = tuple(int(part) for part in version.split('.'))
    for sim in value:
      if sim['state'] == 'Booted' and version >= _MIN_VERSION:
        return (sim['udid'], True)
      if (not largest_version or version > largest_version and
         sim['availability'] == '(available)'):
        largest_version = version
        largest_uuid = sim['udid']

  if not largest_uuid:
    raise KeyError('Unable to find a usable simulator')

  return (largest_uuid, False)


def _InstallApp(uuid, app_dir, is_booted):
  """Installs the given app on the simulator."""
  if not is_booted:
    subprocess.check_call(['xcrun', 'simctl', 'boot', uuid])
  subprocess.check_call(['xcrun', 'simctl', 'install', uuid, app_dir])


def _StartTests(uuid, log_path):
  """Launches the test app on the simulator."""
  subprocess.check_call(['xcrun', 'simctl', 'terminate', uuid, _APP_NAME])
  env = os.environ.copy()
  for k, v in os.environ.items():
    if k.startswith('GTEST') or k.startswith('GLOG'):
      env['SIMCTL_CHILD_' + k] = v

  subprocess.check_call(['xcrun', 'simctl', 'launch', '--stdout=' + log_path,
                         '--stderr=' + log_path, uuid, _APP_NAME],
                        env=env)


def _PollLog(log_path):
  """Polls the log file and waits for the tests to exit."""
  proc = subprocess.Popen(['tail', '-f', log_path], stdout=subprocess.PIPE)
  while True:
    line = proc.stdout.readline().strip()
    if line == _PASS_SIGNAL_LINE:
      return 0
    elif line == _FAIL_SIGNAL_LINE:
      return 1
    else:
      print(line, file=sys.stderr)


def RunTests():
  """Installs the given app, runs the tests, and prints the logs."""
  log_path = os.path.abspath('ios_tests.log')
  try:
    os.remove(log_path)
  except OSError:
    pass

  with open(os.devnull, 'w') as null:
    with _TerminateProcess(subprocess.Popen([_SIMULATOR_PATH], stdout=null,
                                            stderr=null)):
      uuid, is_booted = _FindIosSimulator()
      _InstallApp(uuid, 'tests.app', is_booted)
      _StartTests(uuid, log_path)
      return _PollLog(log_path)


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)

  ns = parser.parse_args(args)
  return RunTests()


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
