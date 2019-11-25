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


"""Generates the shaka config.h file."""

import argparse
import os
import sys

import embed_utils


def _WriteFlag(writer, arg, name):
  if arg:
    writer.Write('#define %s 1' % name)
  else:
    writer.Write('#undef %s' % name)


def _GenConfig(parsed_args, out):
  writer = embed_utils.CodeWriter(out)
  _WriteFlag(writer, parsed_args.sdl_audio, 'SHAKA_SDL_AUDIO')
  _WriteFlag(writer, parsed_args.sdl_video, 'SHAKA_SDL_VIDEO')
  _WriteFlag(writer, parsed_args.media_player, 'SHAKA_DEFAULT_MEDIA_PLAYER')


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--output', required=True,
                      help='The path to the file to generate.')
  parser.add_argument('--sdl-audio', action='store_true',
                      help='Include SdlAudioRenderer in the library.')
  parser.add_argument('--sdl-video', action='store_true',
                      help='Include Sdl*VideoRenderer in the library.')
  parser.add_argument('--media-player', action='store_true',
                      help='Include the default MediaPlayer in the library.')

  parsed_args = parser.parse_args(argv)

  output_dir = os.path.dirname(parsed_args.output)
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)
  with open(parsed_args.output, 'w') as f:
    _GenConfig(parsed_args, f)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
