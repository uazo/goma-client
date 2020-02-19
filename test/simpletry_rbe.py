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
    self._cwd = os.getcwd()
    self._goma._StartCompilerProxy()

  def tearDown(self):
    self._goma._ShutdownCompilerProxy()
    if not self._goma._WaitCooldown():
      self._goma._env.KillStakeholders()

    os.chdir(self._cwd)

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

  @unittest.skipIf(platform.system() != 'Linux', 'javac is used on Linux only')
  def testJavac(self):
    test_dir = os.path.relpath(
        os.path.abspath(os.path.dirname(__file__)), self._dir)

    os.chdir(self._dir)

    # Create input file list and expected output files.
    src_dir = os.path.join(test_dir, 'src')
    input_files = [os.path.join(src_dir, f) for f in ['Foo.java', 'Bar.java']]

    with tempfile.TemporaryDirectory(dir=self._dir) as temp_path:
      temp_path = os.path.relpath(temp_path, self._dir)
      files_list = os.path.join(temp_path, 'files_list.txt')
      with open(files_list, 'w') as fp:
        fp.write(' '.join(os.path.relpath(f, self._dir) for f in input_files))

      # Create output dirs.
      output_dir = os.path.join(temp_path, 'output')
      os.mkdir(output_dir)

      # Run locally: javac -d output @files_list.txt
      jdk_dir = os.path.join(test_dir, '..', 'third_party', 'jdk')
      javac_path = os.path.join(jdk_dir, 'current', 'bin', 'javac')
      javac_cmd = [javac_path, '-d', output_dir, '@%s' % files_list]

      result = subprocess.run(javac_cmd, capture_output=True)
      self.assertEqual(len(result.stdout), 0, msg=result.stdout)
      self.assertEqual(len(result.stderr), 0, msg=result.stderr)

      # One output .class file for each input .java file.
      output_files = ['Foo.class', 'Bar.class']

      output_contents = {}
      for output_file in output_files:
        output_file = os.path.join(output_dir, output_file)
        self.assertTrue(os.path.exists(output_file), msg=output_file)
        output_contents[output_file] = _ReadFileContents(output_file)
        os.remove(output_file)
        self.assertFalse(os.path.exists(output_file), msg=output_file)

      # Run remotely: javac -d output @files_list.txt
      gomacc_path = os.path.join(self._dir, 'gomacc')
      gomacc_cmd = [gomacc_path]
      gomacc_cmd.extend(javac_cmd)

      result = subprocess.run(gomacc_cmd, capture_output=True)
      self.assertEqual(len(result.stdout), 0, msg=result.stdout)
      self.assertEqual(len(result.stderr), 0, msg=result.stderr)

      # Remote output file contents must match local output file contents.
      for output_file in output_files:
        output_file = os.path.join(output_dir, output_file)
        self.assertTrue(os.path.exists(output_file), msg=output_file)
        remote_output_contents = _ReadFileContents(output_file)
        self.assertEqual(
            output_contents.get(output_file),
            remote_output_contents,
            msg=output_file)

    # Check that the remote execution was actually successful, and not a local
    # fallback due to setup failure, which would result in a false positive by
    # generating the outputs locally.
    #
    # We do not have to check for false positives from remote exec failures
    # because of GOMA_USE_LOCAL=false.
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
  # TODO: Use production mixer and backend.
  os.environ['GOMA_SERVER_HOST'] = 'staging-goma.chromium.org'
  os.environ['GOMA_RPC_EXTRA_PARAMS'] = '?staging'
  # Sets environment variables for operation.
  os.environ['GOMA_COMPILER_PROXY_PORT'] = '8100'
  os.environ['GOMA_ARBITRARY_TOOLCHAIN_SUPPORT'] = str(_PlatformSupportsATS())
  os.environ['GOMA_STORE_ONLY'] = 'true'
  os.environ['GOMA_USE_LOCAL'] = 'false'


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
