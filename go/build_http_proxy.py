#!/usr/bin/env python
# Copyright 2020 The Goma Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to build HTTP/1.1 to HTTP/2.0 proxy."""

import argparse
import os
import subprocess
import sys

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_TOP_DIR = os.path.dirname(_SCRIPT_DIR)


def main():
  parser = argparse.ArgumentParser(description='build http proxy')
  parser.add_argument('--output', help='proxy filename', required=True)
  parser.add_argument(
      '--cache-dir', help='gocache directory name', required=True)
  args = parser.parse_args()

  gopath = os.path.join(_TOP_DIR, 'third_party', 'go', 'bin', 'go')
  env = {
      'CGO_ENABLED': '0',
      'GOCACHE': args.cache_dir,
  }
  subprocess.check_call([
      gopath, 'build', '-o', args.output,
      os.path.join(_SCRIPT_DIR, 'proxy', 'proxy.go')
  ],
                        env=env)
  return 0


if __name__ == '__main__':
  sys.exit(main())
