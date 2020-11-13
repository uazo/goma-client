#!/usr/bin/env python

# Copyright 2020 Google LLC.
"""Tests for goma_auth."""

from __future__ import print_function

import imp
import argparse
import os
import sys
import unittest

_GOMA_AUTH = 'goma_auth.py'


def _ClearGomaEnv():
  """Clear goma-related environmental variables."""
  to_delete = []
  for e in os.environ:
    if e.startswith('GOMA_'):
      to_delete.append(e)
  for e in to_delete:
    del os.environ[e]


class GomaAuthTest(unittest.TestCase):
  """Common features for goma_auth.py test."""

  # test should be able to access protected members and variables.
  # pylint: disable=W0212

  def __init__(self, method_name, goma_auth_path):
    """Initialize GomaAuthTest.

    To be ready for accidentally write files in a test, initializer will
    create a directory for test.

    Args:
      method_name: a string of test method name to execute.
      goma_auth_path: a string of goma directory name.
    """
    super(GomaAuthTest, self).__init__(method_name)
    self._goma_auth_path = goma_auth_path

  def setUp(self):
    _ClearGomaEnv()

    mod_name, _ = os.path.splitext(_GOMA_AUTH)
    self._module = imp.load_source(
        mod_name, os.path.join(self._goma_auth_path, _GOMA_AUTH))

  def tearDown(self):
    _ClearGomaEnv()

  def testRestartCompilerProxyShouldRun(self):
    config = self._module.GomaOAuth2Config()
    self._module.FetchTokenInfo = lambda x: {'email': 'fake@google.com'}
    self._module.CheckPing = lambda x: True
    flags = self._module.ConfigFlags(config)
    self.assertEqual(
        flags.get('GOMA_SERVER_HOST'), self._module.DEFAULT_GOMA_SERVER_HOST)
    self.assertEqual(flags.get('GOMACTL_USE_PROXY'), 'true')


def GetParameterizedTestSuite(klass, **kwargs):
  test_loader = unittest.TestLoader()
  test_names = test_loader.getTestCaseNames(klass)
  suite = unittest.TestSuite()
  for name in test_names:
    suite.addTest(klass(name, **kwargs))
  return suite


def main():
  test_dir = os.path.abspath(os.path.dirname(__file__))
  os.chdir(os.path.join(test_dir, '..'))

  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--goma-dir', required=True, help='absolute or relative to goma top dir')
  parser.add_argument('--verbosity', default=1, help='Verbosity of tests.')
  args = parser.parse_args()

  print('testdir:%s' % test_dir)
  goma_auth_path = os.path.abspath(os.path.join(test_dir, args.goma_dir))

  # Execute test.
  suite = unittest.TestSuite()
  suite.addTest(
      GetParameterizedTestSuite(GomaAuthTest, goma_auth_path=goma_auth_path))
  result = unittest.TextTestRunner(verbosity=args.verbosity).run(suite)

  # Return test status as exit status.
  exit_code = 0
  if result.errors:
    exit_code |= 0x01
  if result.failures:
    exit_code |= 0x02
  if exit_code:
    sys.exit(exit_code)


if __name__ == '__main__':
  main()
