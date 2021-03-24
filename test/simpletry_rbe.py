#!/usr/bin/env vpython3
# Copyright 2020 The Goma Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple test scripts for sanity check.

The script is intended to be used with the production Goma RBE server.
"""

import argparse
import os
import platform
import socket
import string
import subprocess
import sys
import tempfile
import time
import unittest
import urllib.request

from importlib import import_module

_GOMA_CTL = 'goma_ctl.py'
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def _PlatformSupportsATS():
  return platform.system() != 'Darwin'


def _ReadFileContents(path):
  """Returns contents of file at `path`"""
  with open(path, 'rb') as fp:
    return fp.read()


def _ReadFromCompilerProxyPath(path):
  """Returns HTTP response from localhost:${GOMA_COMPILER_PROXY_PORT}/${path}"""
  port = os.environ['GOMA_COMPILER_PROXY_PORT']
  url = 'http://localhost:%s/%s' % (port, path)
  with urllib.request.urlopen(url) as response:
    return response.read().decode('utf-8')


class RunCompilerProxyTest(unittest.TestCase):
  """Goma Simple Try Test: Test running of compiler_proxy."""

  def __init__(self, method_name, goma_dir):
    """Initialize.

    Args:
      method_name: a string of method name to test.
      goma_dir: a string of GOMA directory.
    """
    super(RunCompilerProxyTest, self).__init__(method_name)
    self._dir = os.path.abspath(goma_dir)

    # Find and load goma_ctl module.
    sys.path.append(self._dir)
    mod_name, _ = os.path.splitext(_GOMA_CTL)
    self._module = import_module(mod_name)
    self._goma = self._module.GetGomaDriver()

  def setUp(self):
    if not 'AUTONINJA_BUILD_ID' in os.environ:
      if 'LOGDOG_STREAM_PREFIX' in os.environ:
        os.environ['AUTONINJA_BUILD_ID'] = 'simpletry_rbe/' + os.environ.get(
            'LOGDOG_STREAM_PREFIX')
      else:
        os.environ['AUTONINJA_BUILD_ID'] = 'simpletry_rbe/%s/%d/%d' % (
            socket.gethostname(), os.getpid(), time.time())
    print('\nAUTONINJA_BUILD_ID=%s\n' % os.environ.get('AUTONINJA_BUILD_ID'))
    self._cwd = os.getcwd()
    self._goma._StartCompilerProxy()

  def tearDown(self):
    self._goma._ShutdownCompilerProxy()
    if not self._goma._WaitCooldown():
      self._goma._env.KillStakeholders()
    # TODO:  print log only if test failed?
    infolog_path = self._goma._env.FindLatestLogFile('compiler_proxy', 'INFO')
    if not infolog_path:
      print('compiler_proxy log was not found')
    else:
      with open(infolog_path) as f:
        sys.stdout.write(f.read())

    os.chdir(self._cwd)

  def AssertSameContents(self, a, b, msg=''):
    """Asserts given two filecontents are the same.

    Args:
      a, b:  two file contents to check.
      msg: additional message to be shown.
    """
    if a == b:
      return  # Success.

    self.assertEqual(
        len(a),
        len(b),
        msg=('%ssize mismatch: a=%d b=%d' % (msg, len(a), len(b))))
    idx = -1
    ndiff = 0
    for ach, bch in zip(a, b):
      idx += 1
      # http://support.microsoft.com/kb/121460/en
      # Header structure (0 - 20 bytes):
      #  0 -  2: Machine
      #  2 -  4: Number of sections.
      #  4 -  8: Time/Date Stamp.
      #  8 - 12: Pointer to Symbol Table.
      # 12 - 16: Number of Symbols.
      # 16 - 18: Optional Header Size.
      # 18 - 20: Characteristics.
      if idx in range(4, 8):  # Time/Date Stamp can be different.
        continue
      # Since compiler_proxy normalize path names to lower case, we should
      # normalize printable charactors before comparison.
      ach = '%c' % ach
      if ach in string.printable:
        ach = ach.lower()
      bch = '%c' % bch
      if bch in string.printable:
        bch = bch.lower()

      if ach != bch:
        ndiff += 1
    print('%d bytes differ' % ndiff)
    self.assertEqual(
        ndiff, 0, msg=('%sobj file should be the same after normalize.' % msg))

  def testFlagz(self):
    # This ensures that compiler_proxy is healthy by reading from /flagz.
    # It further ensures that /flagz had a valid output by checking that flag
    # values match values that were passed from environment variables.
    flags = _ReadFromCompilerProxyPath('flagz')
    for flag in flags.split('\n'):
      if len(flag) == 0:  # skip empty lines
        continue
      flag_key, flag_value = flag.split('=', 1)
      self.assertTrue(
          flag_key.startswith('GOMA_'),
          '%s does not start with "GOMA_"' % flag_key)
      self.assertIsNotNone(flag_value)
      if flag_key in os.environ:
        self.assertEqual(flag_value, os.environ.get(flag_key))

  # TODO: Add separate test for Windows using clang-cl.
  @unittest.skipIf(platform.system() == 'Windows', 'clang for non-Windows only')
  def testClang(self):
    test_dir = os.path.abspath(os.path.dirname(__file__))
    test_dir = os.path.relpath(test_dir, self._dir)
    temp_dir = os.path.relpath(tempfile.mkdtemp(dir=self._dir), self._dir)

    os.chdir(self._dir)

    # Run locally: clang -c foo.cc -o foo.o
    clang_path = os.path.join(test_dir, '..', 'third_party', 'llvm-build',
                              'Release+Asserts', 'bin', 'clang')
    input_path = os.path.join(test_dir, 'src', 'foo.cc')
    output_path = os.path.join(temp_dir, 'foo.o')
    sysroot_path = os.path.join(test_dir, '..', 'third_party', 'chromium_build',
                                'linux', 'debian_sid_amd64-sysroot', 'usr',
                                'lib', 'gcc', 'x86_64-linux-gnu', '6')
    clang_cmd = [
        clang_path, '-c', input_path, '-o', output_path, '-g', '--sysroot',
        sysroot_path
    ]

    result = subprocess.run(clang_cmd, capture_output=True)
    self.assertEqual(len(result.stdout), 0, msg=result.stdout)
    self.assertEqual(len(result.stderr), 0, msg=result.stderr)
    self.assertTrue(os.path.exists(output_path))

    # Save output file contents and delete it.
    local_output_contents = _ReadFileContents(output_path)
    os.remove(output_path)
    self.assertFalse(os.path.exists(output_path))

    # Run remotely: clang -c hello.c -o hello.o
    gomacc_path = os.path.join(self._dir, 'gomacc')
    gomacc_cmd = [gomacc_path]
    gomacc_cmd.extend(clang_cmd)

    result = subprocess.run(gomacc_cmd, capture_output=True)
    self.assertEqual(len(result.stdout), 0, msg=result.stdout)
    self.assertEqual(len(result.stderr), 0, msg=result.stderr)

    # Remote output file contents must match local output file contents.
    self.assertTrue(os.path.exists(output_path))
    remote_output_contents = _ReadFileContents(output_path)

    self.assertEqual(local_output_contents, remote_output_contents)

    # Verify that this was not a false positive due to fallback after setup
    # failure.
    statz = _ReadFromCompilerProxyPath('statz')
    self.assertNotIn('fallback by setup failure', statz, statz)
    self.assertIn('success=1 failure=0', statz, statz)

  @unittest.skipIf(platform.system() != 'Linux',
                   'clang-cl (not clang-cl.exe) for Linux only')
  def testClangCl(self):
    test_dir = os.path.abspath(os.path.dirname(__file__))
    test_dir = os.path.relpath(test_dir, self._dir)
    temp_dir = os.path.relpath(tempfile.mkdtemp(dir=self._dir), self._dir)

    os.chdir(self._dir)

    # Run locally: clang-cl -c foo.cc -o foo.o
    clang_path = os.path.join(test_dir, '..', 'third_party', 'llvm-build',
                              'Release+Asserts', 'bin', 'clang-cl')
    input_path = os.path.join(test_dir, 'src', 'foo.cc')
    output_path = os.path.join(temp_dir, 'foo.o')
    clang_cmd = [
        clang_path,
        '-c',
        input_path,
        '-o',
        output_path,
    ]

    result = subprocess.run(clang_cmd, capture_output=True)
    self.assertEqual(len(result.stdout), 0, msg=result.stdout)
    self.assertEqual(len(result.stderr), 0, msg=result.stderr)
    self.assertTrue(os.path.exists(output_path))

    # Save output file contents and delete it.
    local_output_contents = _ReadFileContents(output_path)
    os.remove(output_path)
    self.assertFalse(os.path.exists(output_path))

    # Run remotely: clang -c hello.c -o hello.o
    gomacc_path = os.path.join(self._dir, 'gomacc')
    gomacc_cmd = [gomacc_path]
    gomacc_cmd.extend(clang_cmd)

    result = subprocess.run(gomacc_cmd, capture_output=True)
    self.assertEqual(len(result.stdout), 0, msg=result.stdout)
    self.assertEqual(len(result.stderr), 0, msg=result.stderr)

    # Remote output file contents must match local output file contents.
    self.assertTrue(os.path.exists(output_path))
    remote_output_contents = _ReadFileContents(output_path)

    self.AssertSameContents(local_output_contents, remote_output_contents)

    # Verify that this was not a false positive due to fallback after setup
    # failure.
    statz = _ReadFromCompilerProxyPath('statz')
    self.assertNotIn('fallback by setup failure', statz, statz)
    self.assertIn('success=1 failure=0', statz, statz)


def GetParameterizedTestSuite(klass, **kwargs):
  """Make test suite parameterized.

  Args:
    klass: a subclass of unittest.TestCase.
    kwargs: arguments given to klass.

  Returns:
    an instance of unittest.TestSuite for |klass|.
  """
  test_loader = unittest.TestLoader()
  test_names = test_loader.getTestCaseNames(klass)
  suite = unittest.TestSuite()
  for name in test_names:
    suite.addTest(klass(name, **kwargs))
  return suite


def SetEnvVars():
  # Sets environment variables for connection.
  os.environ['GOMA_SERVER_HOST'] = 'goma.chromium.org'
  os.environ['GOMA_RPC_EXTRA_PARAMS'] = '?prod'
  # Sets environment variables for operation.
  os.environ['GOMA_COMPILER_PROXY_PORT'] = '8100'
  os.environ['GOMA_ARBITRARY_TOOLCHAIN_SUPPORT'] = str(_PlatformSupportsATS())
  os.environ['GOMA_STORE_ONLY'] = 'true'
  os.environ['GOMA_USE_LOCAL'] = 'false'
  os.environ['GOMACTL_USE_PROXY'] = 'false'


def ExecuteTests(goma_dir):
  """Execute Tests.

  Args:
    goma_dir: a string of goma directory.

  Returns:
    integer exit code representing test status.  (success == 0)
    0x01: there is errors.
    0x02: there is failures.
  """

  # starts test.
  suite = unittest.TestSuite()
  suite.addTest(
      GetParameterizedTestSuite(RunCompilerProxyTest, goma_dir=goma_dir))
  result = unittest.TextTestRunner(verbosity=2).run(suite)

  # Return test status as exit status.
  exit_code = 0
  if result.errors:
    exit_code |= 0x01
  if result.failures:
    exit_code |= 0x02
  return exit_code


def main():
  test_dir = os.path.abspath(os.path.dirname(__file__))
  os.chdir(os.path.join(test_dir, '..'))

  arg_parser = argparse.ArgumentParser()
  arg_parser.add_argument(
      '--goma-dir',
      default=os.path.join(test_dir, '..', 'out', 'Release'),
      help=('directory with goma executables, can be absolute or relative to ' +
            '%s' % os.path.basename(__file__)))

  args = arg_parser.parse_args()
  SetEnvVars()
  goma_dir = os.path.abspath(args.goma_dir)

  exit_code = ExecuteTests(goma_dir)

  if exit_code:
    sys.exit(exit_code)


if __name__ == '__main__':
  main()
