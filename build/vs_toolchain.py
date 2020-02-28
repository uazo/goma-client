#!/usr/bin/env python
# Copyright 2020 The Goma Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# //build/vs_toolchain.py cannot be fully removed, because some
# other scripts rely on the file structure of //build/vs_toolchain.py
# (e.g. //tools/clang/scripts/update.py). This file imports all the
# symbols from //third_party/chromium_build/vs_toolchain.py, so
# we don't have to roll this file up anymore.
import imp
import os
import sys

# All the tricks of manipulating sys.path to resolve which vs_toolchain.py
# to import have failed to work for these two cases at the same time:
# * Use this file as an executable script
# * Import this file as a module
#
# The impl module solved this problem.
# See https://stackoverflow.com/a/10161654
_THIS_DIR = os.path.dirname(__file__)
_MODULE_PATH = os.path.abspath(
    os.path.join(_THIS_DIR, '..', 'third_party', 'chromium_build',
                 'vs_toolchain.py'))
_vs_toolchain = imp.load_source('_vs_toolchain', _MODULE_PATH)
from _vs_toolchain import *


if __name__ == '__main__':
  sys.exit(main())
