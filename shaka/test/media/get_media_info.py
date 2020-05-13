#!/usr/bin/env python3
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

"""Reads the given file and stores expected frame info about it.

This uses ffprobe to determine the correct resulting data.  If using fragmented
files, you need to specify the init segment.
"""

from __future__ import print_function

import argparse
import os
import subprocess
import sys
import tempfile

_TEST_MEDIA_BASE = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'media')
sys.path.append(_TEST_MEDIA_BASE)
try:
  import media_tests_pb2
except ImportError:
  subprocess.check_call(['protoc', 'media_tests.proto', '--python_out=.'],
                        cwd=_TEST_MEDIA_BASE)
  import media_tests_pb2


def _parse_ffprobe(data):
  """Parses ffprobe output into an array of dicts."""
  ret = []
  cur = None
  skip = False
  for line in data.strip().split('\n'):
    line = line.strip()
    if line in {'[PACKET]', '[STREAM]'}:
      assert cur is None
      cur = dict()
      ret.append(cur)
    elif line in {'[/PACKET]', '[/STREAM]'}:
      cur = None
    elif line == '[SIDE_DATA]':
      assert cur is not None
      assert not skip
      skip = True
    elif line == '[/SIDE_DATA]':
      assert cur is not None
      assert skip
      skip = False
    elif skip and 'Encryption info' in line:
      cur['is_encrypted'] = True
    elif line and not skip:
      assert cur is not None
      key, value = line.split('=', 1)
      cur[key] = value

  assert cur is None
  return ret


def _probe_stream(path, stream):
  """Probes the given file and creates a Stream proto.

  Args:
    path: The path to the file to probe.
    stream: The Stream proto to fill.
  """
  try:
    output = subprocess.check_output(
        ['ffprobe', '-show_streams', '-show_data_hash', 'md5', path],
        stderr=subprocess.DEVNULL).decode('utf8')
  except subprocess.CalledProcessError as e:
    print(e.output, file=sys.stderr)
    raise

  streams = _parse_ffprobe(output)
  assert len(streams) == 1
  num, den = streams[0]['time_base'].split('/')
  stream.time_scale_num = int(num)
  stream.time_scale_den = int(den)
  stream.is_video = streams[0]['codec_type'] == 'video'
  stream.extra_data_hash = streams[0]['extradata_hash'][4:].upper()
  num, den = streams[0]['sample_aspect_ratio'].split(':')
  stream.sar_num = int(num)
  stream.sar_den = int(den)


def _probe_frames(path, frames):
  """Probes the given file and creates Frame protos.

  Args:
    path: A file path to the file to parse.
    frames: A repeatable Frame proto to put frames into.
  """
  try:
    output = subprocess.check_output(
        ['ffprobe', '-show_packets', '-show_data_hash', 'md5', path],
        stderr=subprocess.DEVNULL).decode('utf8')
  except subprocess.CalledProcessError as e:
    print(e.output, file=sys.stderr)
    raise

  packets = _parse_ffprobe(output)
  for packet in packets:
    cur = frames.add()
    cur.pts = float(packet['pts_time'])
    cur.dts = float(packet['dts_time'])
    if packet['duration_time'] != 'N/A':
      cur.duration = float(packet['duration_time'])
    cur.data_hash = packet['data_hash'][4:].upper()
    cur.is_encrypted = packet.get('is_encrypted', False)


def _probe(info, path, init_path):
  """Probes the given file to get the frame info.

  Args:
    info: The MediaInfo proto to put info into.
    path: The path to the input file to probe.
    init_path: If not None, the path to an init segment.
  """
  with tempfile.NamedTemporaryFile() as temp:
    if init_path:
      with open(init_path, 'rb') as init:
        temp.write(init.read())
    with open(path, 'rb') as f:
      temp.write(f.read())
    temp.flush()

    _probe_stream(temp.name, info.stream)
    _probe_frames(temp.name, info.frames)


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--mime', required=True,
                      help='The MIME type of the content.')
  parser.add_argument('--init', help='The path to the init segment.')
  parser.add_argument('file', help='The path to the file to read.')

  parsed_args = parser.parse_args(args)

  ret = media_tests_pb2.MediaInfo()
  ret.mime = parsed_args.mime
  _probe(ret, parsed_args.file, parsed_args.init)

  with open(parsed_args.file + '.dat', 'wb') as f:
    f.write(ret.SerializeToString())

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
