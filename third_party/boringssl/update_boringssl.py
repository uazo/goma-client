#!/usr/bin/env python3
# Copied from Chromium src/third_party/boringssl, and modified for Goma.
# - We do not build/run tests.
# - We do not need some architectures, which is listed in .gitignore.
# - We should not contaminate Chromium issue.
# - It follows existing boringssl update commit description flavor.
# - It automatically removes removed asm files.
# forked from roll_boringssl.py.
# - This program only update files. No DEPS update, no git commands.
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates generated build files for third_party/boringssl/src."""

import os
import os.path
import shutil
import subprocess
import sys

SCRIPT_PATH = os.path.abspath(__file__)
SRC_PATH = os.path.dirname(os.path.dirname(os.path.dirname(SCRIPT_PATH)))
BORINGSSL_INSIDE_REPO_PATH = os.path.join('third_party', 'boringssl')
BORINGSSL_PATH = os.path.join(SRC_PATH, BORINGSSL_INSIDE_REPO_PATH)
BORINGSSL_SRC_PATH = os.path.join(BORINGSSL_PATH, 'src')

assert sys.version_info.major == 3, ('Non supported python version. '
                                     'Please run with python3.')

assert os.path.isdir(BORINGSSL_SRC_PATH), 'Could not find BoringSSL checkout'

# Pull OS_ARCH_COMBOS out of the BoringSSL script.
sys.path.append(os.path.join(BORINGSSL_SRC_PATH, 'util'))
import generate_build_files

GENERATED_FILES = [
    'BUILD.generated.gni',
    'err_data.c',
]


def main():
  # Clear the old generated files.
  for (osname, arch, _, _, _) in generate_build_files.OS_ARCH_COMBOS:
    path = os.path.join(BORINGSSL_PATH, osname + '-' + arch)
    try:
      shutil.rmtree(path)
    except FileNotFoundError as e:
      print('file to be removed has already been removed %s: %s' % (path, e))
  for f in GENERATED_FILES:
    path = os.path.join(BORINGSSL_PATH, f)
    os.unlink(path)

  # Generate new ones.
  subprocess.check_call([
      sys.executable,
      os.path.join(BORINGSSL_SRC_PATH, 'util', 'generate_build_files.py'), 'gn'
  ],
                        cwd=BORINGSSL_PATH)

  # Remove dirs in .gitignore.
  gitignore = []
  with open(os.path.join(BORINGSSL_PATH, '.gitignore')) as f:
    gitignore = f.read().splitlines()
  for entry in gitignore:
    path = os.path.join(BORINGSSL_PATH, entry)
    try:
      if os.path.isfile(path):
        os.unlink(path)
      else:
        shutil.rmtree(path)
    except FileNotFoundError as e:
      print('file to be removed has already been removed %s: %s' % (path, e))

  return 0


if __name__ == '__main__':
  sys.exit(main())
