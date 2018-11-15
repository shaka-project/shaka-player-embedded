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


"""Runs ./configure to generate required files.

This is run as an action by GN for a specific target.  This will generated files
in a directory in the build tree.  This ensures that different configurations
will use different generated files without contaminating each other.
"""

import argparse
import os
import subprocess
import sys


_TOOLS_DIR = os.path.dirname(os.path.abspath(os.path.realpath(__file__)))
_THIRD_PARTY_DIR = os.path.join(_TOOLS_DIR, '..', '..', 'third_party')


def FlagsForCpu(cpu, target_os, sysroot):
  """Returns the flags to build for the given CPU."""
  FLAGS = {
      'x64': '-m64',
      'x86': '-m32',
      'arm': '-arch armv7',
      'arm64': '-arch arm64',
  }
  assert ' ' not in sysroot, "sysroot path can't contain spaces"
  assert cpu in FLAGS, 'Unsupported CPU architecture'

  extra = ' -isysroot ' + sysroot if sysroot else ''
  if target_os == 'ios':
    if cpu == 'x86' or cpu == 'x64':
      extra += ' -mios-simulator-version-min=9.0'
    else:
      extra += ' -miphoneos-version-min=9.0'
  return FLAGS[cpu] + extra


def _GetHost(cpu, target_os):
  """Returns the host triple for the given OS and CPU."""
  if cpu == 'x64':
    cpu = 'x86_64'
  elif cpu == 'arm64':
    cpu = 'aarch64'

  if target_os == 'linux':
    return cpu + '-unknown-linux'
  elif target_os == 'mac':
    return cpu + '-apple-darwin'
  elif target_os == 'ios':
    return cpu + '-ios-darwin'
  else:
    raise RuntimeError('Unsupported host')


def MakeClean(src_dir):
  """Runs |make distclean| to clean up old configurations."""
  # Ignore bad return code (e.g. errors for missing Makefile).
  with open(os.devnull, 'w') as null:
    subprocess.call(['make', 'clean'], cwd=src_dir, stdout=null, stderr=null)
    subprocess.call(['make', 'distclean'], cwd=src_dir, stdout=null,
                    stderr=null)


def CrossConfigure(src_dir, out_dir, cpu, target_os, sysroot, extra_flags):
  """Runs a configure script for cross-compiling with the given options.

  Arguments:
    src_dir: The path of the source code.
    out_dir: The path to put the generated files.
    cpu: The CPU to build for.
    target_os: The OS to build for.
    sysroot: The path to the compiler sysroot (optional).
    extra_flags: A list of extra flags to pass.

  Returns:
    The configure process' return code.
  """
  # Create a relative path so the absolute path can contain spaces.
  third_party_rel = os.path.relpath(_THIRD_PARTY_DIR, os.path.abspath(out_dir))
  prefix = os.path.join(third_party_rel, 'llvm-build', 'Release+Asserts', 'bin')
  assert ' ' not in prefix, "Path to compiler can't contain spaces"
  clang_flags = FlagsForCpu(cpu, target_os, sysroot)
  flags = extra_flags
  flags += [
      '--srcdir=' + os.path.relpath(src_dir, os.path.abspath(out_dir)),
      '--host=' + _GetHost(cpu, target_os),
      'CC=%s/clang %s' % (prefix, clang_flags),
      'CPP=%s/clang -dM -E %s' % (prefix, clang_flags),
      'CXX=%s/clang++ %s' % (prefix, clang_flags),
      'CXXCPP=%s/clang++ -dM -E %s' % (prefix, clang_flags),
  ]

  if not os.path.exists(out_dir):
    os.makedirs(out_dir)
  exe = os.path.join(os.path.abspath(src_dir), 'configure')
  proc = subprocess.Popen([exe] + flags,
                          cwd=out_dir,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT)

  stdout, _ = proc.communicate()
  if proc.returncode != 0:
    print >> sys.stderr, stdout
  return proc.returncode
