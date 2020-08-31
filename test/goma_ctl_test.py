#!/usr/bin/env python

# Copyright 2012 The Goma Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for goma_ctl."""

from __future__ import print_function

import imp
import json
import optparse
import os
import shutil
import stat
import string
import sys
import tempfile
import time
import unittest

# TODO: remove this when we deprecate python2.
if sys.version_info >= (3, 0):
  import io
  STRINGIO = io.StringIO
else:
  import cStringIO
  STRINGIO = cStringIO.StringIO

_GOMA_CTL = 'goma_ctl.py'


class PlatformSpecific(object):
  """class for platform specific commands / data."""

  def __init__(self, platform):
    self._platform = platform

  def GetPlatform(self):
    """Returns platform name."""
    return self._platform

  @staticmethod
  def GetDefaultGomaCtlPath(test_dir):
    """Returns platform name.

    Args:
      test_dir: a string of directory of this file.

    Returns:
      a string of the directory contains goma_ctl.py by default.
    """
    raise NotImplementedError('GetDefaultGomaCtlPath should be implemented.')

  @staticmethod
  def SetCompilerProxyEnv(tmp_dir, port):
    """Configure compiler_proxy env.

    Args:
      tmp_dir: a string of temporary directory path.
      port: an integer of compiler proxy port.
    """
    raise NotImplementedError('GetDefaultGomaCtlPath should be implemented.')

  def GetCred(self):
    if os.path.isfile(self._CRED):
      return self._CRED
    return None


class WindowsSpecific(PlatformSpecific):
  """class for Windows specific commands / data."""

  _CRED = 'c:\\creds\\service_accounts\\service-account-goma-client.json'

  @staticmethod
  def GetDefaultGomaCtlPath(test_dir):
    return os.path.join(test_dir, '..', 'out', 'Release')

  @staticmethod
  def SetCompilerProxyEnv(tmp_dir, port):
    os.environ['GOMA_TMP_DIR'] = tmp_dir
    os.environ['GOMA_COMPILER_PROXY_PORT'] = str(port)


class PosixSpecific(PlatformSpecific):
  """class for Windows specific commands / data."""

  _CRED = '/creds/service_accounts/service-account-goma-client.json'

  @staticmethod
  def GetDefaultGomaCtlPath(test_dir):
    return os.path.join(test_dir, '..', 'out', 'Release')

  @staticmethod
  def SetCompilerProxyEnv(tmp_dir, port):
    os.environ['GOMA_TMP_DIR'] = tmp_dir
    os.environ['GOMA_COMPILER_PROXY_PORT'] = str(port)


def GetPlatformSpecific(platform):
  """Get PlatformSpecific class for |platform|.

  Args:
    platform: platform name to be returned.

  Returns:
    an instance of a subclass of PlatformSpecific class.

  Raises:
    ValueError: if platform is None or not supported.
  """
  if platform in ('win', 'win64'):
    return WindowsSpecific(platform)
  elif platform in ('linux', 'mac', 'goobuntu'):
    return PosixSpecific(platform)
  raise ValueError('You should specify supported platform name.')


class FakeGomaEnv(object):
  """Fake GomaEnv class for test."""
  # pylint: disable=R0201
  # pylint: disable=W0613

  def CalculateChecksum(self, _):
    return 'dummy_checksum'

  def CheckAuthConfig(self):
    pass

  def CheckConfig(self):
    pass

  def CheckRBEDogfood(self):
    pass

  def ControlCompilerProxy(self, command, check_running=True, need_pids=False):
    if command == '/healthz':
      return {'status': True, 'message': 'ok', 'url': 'dummy_url',
              'pid': 'unknown'}
    return {'status': True, 'message': 'dummy', 'url': 'dummy_url',
            'pid': 'unknown'}

  def CompilerProxyBinary(self):
    return os.path.normpath('/path/to/compiler_proxy')

  def CompilerProxyRunning(self):
    return True

  def ExecCompilerProxy(self):
    pass

  @staticmethod
  def FindLatestLogFile(command_name, log_type):
    return 'dummy_info'

  def GetCacheDirectory(self):
    return 'dummy_cache_dir'

  def GetCrashDumpDirectory(self):
    return 'dummy_crash_dump_dir'

  def GetCrashDumps(self):
    return []

  def GetCompilerProxyVersion(self):
    return 'fake@version'

  def GetGomaCtlScriptName(self):
    return 'fake-script'

  def GetGomaTmpDir(self):
    return 'dummy_tmp_dir'

  def GetPlatform(self):
    return 'fake'

  def GetScriptDir(self):
    return 'fake'

  def GetUsername(self):
    return 'fakeuser'

  def HttpDownload(self, url):
    return 'fake'

  def IsDirectoryExist(self, _):
    return True

  def EnsureDirectoryOwnedByUser(self, _):
    return True

  def IsOldFile(self, _):
    return True

  def IsProductionBinary(self):
    return True

  def KillStakeholders(self, force=False):
    pass

  def LoadChecksum(self):
    return {}

  def MakeDirectory(self, _):
    pass

  def MayUsingDefaultIPCPort(self):
    return True

  def ReadManifest(self, path=''):
    if path:
      return {'VERSION': 1}
    return {}

  def WriteFile(self, filename, content):
    pass

  def CopyFile(self, from_file, to_file):
    pass

  def MakeTgzFromDirectory(self, dir_name, output_filename):
    pass

  def RemoveFile(self, _):
    pass

  def SetDefaultKey(self, protocol):
    pass


  def WarnNonProtectedFile(self, filename):
    pass


def _ClearGomaEnv():
  """Clear goma-related environmental variables."""
  to_delete = []
  for e in os.environ:
    if e.startswith('GOMA_'):
      to_delete.append(e)
    if e.startswith('GOMACTL_'):
      to_delete.append(e)
  for e in to_delete:
    del os.environ[e]
  if 'GOMAMODE' in os.environ:
    del os.environ['GOMAMODE']
  if 'PLATFORM' in os.environ:
    del os.environ['PLATFORM']


class GomaCtlTestCommon(unittest.TestCase):
  """Common features for goma_ctl.py test."""
  # test should be able to access protected members and variables.
  # pylint: disable=W0212

  _TMP_SUBDIR_NAME = 'goma'

  def __init__(self, method_name, goma_ctl_path, platform_specific):
    """Initialize GomaCtlTest.

    To be ready for accidentally write files in a test, initializer will
    create a directory for test.

    Args:
      method_name: a string of test method name to execute.
      goma_ctl_path: a string of goma directory name.
      platform_specific: a object for providing platform specific behavior.
    """
    super(GomaCtlTestCommon, self).__init__(method_name)
    self._goma_ctl_path = goma_ctl_path
    self._platform_specific = platform_specific

  def setUp(self):
    _ClearGomaEnv()

    # suppress stdout and make it available from test.
    sys.stdout = STRINGIO()

    mod_name, _ = os.path.splitext(_GOMA_CTL)
    # Copy GOMA client commands to a temporary directory.
    # The directory should be removed at tearDown.
    # TODO: copy same files as archive.py?
    self._tmp_dir = tempfile.mkdtemp()
    shutil.copytree(self._goma_ctl_path,
                    os.path.join(self._tmp_dir, self._TMP_SUBDIR_NAME),
                    symlinks=True,
                    ignore=shutil.ignore_patterns('lib', 'lib.target', 'obj',
                                                  'obj.*', '*_unittest*',
                                                  '*proto*', '.deps'))
    self._module = imp.load_source(mod_name,
                                   os.path.join(self._tmp_dir,
                                                self._TMP_SUBDIR_NAME,
                                                _GOMA_CTL))

  def tearDown(self):
    _ClearGomaEnv()
    shutil.rmtree(self._tmp_dir)


class GomaCtlSmallTest(GomaCtlTestCommon):
  """Small tests for goma_ctl.py.

  All tests in this class use test doubles and do not expected to affect
  external environment.
  """
  # test should be able to access protected members and variables.
  # pylint: disable=W0212

  def setUp(self):
    super(GomaCtlSmallTest, self).setUp()
    # Since we use test doubles, we do not have to wait.
    self._module._COOLDOWN_SLEEP = 0

  def CreateSpyControlCompilerProxy(self):
    class SpyGomaEnv(FakeGomaEnv):
      """Spy GomaEnv to capture ControlCompilerProxy command."""

      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.command = ''

      def ControlCompilerProxy(self, command, check_running=True,
                               need_pids=False):
        self.command = command
        return super(SpyGomaEnv, self).ControlCompilerProxy(command,
                                                            check_running,
                                                            need_pids)
    return SpyGomaEnv()

  def testIsGomaFlagTrueShouldShowTrueForVariousTruePatterns(self):
    flag_test_name = 'FLAG_TEST'
    self.assertFalse(self._module._IsFlagTrue(flag_test_name))
    os.environ[flag_test_name] = 'T'
    self.assertTrue(self._module._IsFlagTrue(flag_test_name))
    os.environ[flag_test_name] = 'true'
    self.assertTrue(self._module._IsFlagTrue(flag_test_name))
    os.environ[flag_test_name] = 'y'
    self.assertTrue(self._module._IsFlagTrue(flag_test_name))
    os.environ[flag_test_name] = 'Yes'
    self.assertTrue(self._module._IsFlagTrue(flag_test_name))
    os.environ[flag_test_name] = '1'
    self.assertTrue(self._module._IsFlagTrue(flag_test_name))

  def testIsGomaFlagTrueShouldShowFalseForVariousFalsePatterns(self):
    flag_test_name = 'FLAG_TEST'
    os.environ[flag_test_name] = 'F'
    self.assertFalse(self._module._IsFlagTrue(flag_test_name))
    os.environ[flag_test_name] = 'false'
    self.assertFalse(self._module._IsFlagTrue(flag_test_name))
    os.environ[flag_test_name] = 'n'
    self.assertFalse(self._module._IsFlagTrue(flag_test_name))
    os.environ[flag_test_name] = 'No'
    self.assertFalse(self._module._IsFlagTrue(flag_test_name))
    os.environ[flag_test_name] = '0'
    self.assertFalse(self._module._IsFlagTrue(flag_test_name))

  def testIsGomaFlagTrueShouldFollowDefaultIfEnvNotSet(self):
    flag_test_name = 'FLAG_TEST'
    self.assertFalse(self._module._IsFlagTrue(flag_test_name, default=False))
    self.assertTrue(self._module._IsFlagTrue(flag_test_name, default=True))

  def testSetGomaFlagDefaultValueIfEmptyShouldSetIfEmpty(self):
    flag_test_name = 'FLAG_TEST'
    flag_test_value = 'test'
    self.assertFalse(('GOMA_%s' % flag_test_name) in os.environ)
    self._module._SetGomaFlagDefaultValueIfEmpty(flag_test_name,
                                                 flag_test_value)
    self.assertTrue(('GOMA_%s' % flag_test_name) in os.environ)
    self.assertEqual(os.environ['GOMA_%s' % flag_test_name], flag_test_value)

  def testSetGomaFlagDefaultValueIfEmptyShouldNotSetIfNotEmpty(self):
    flag_test_name = 'FLAG_TEST'
    flag_test_value = 'test'
    flag_orig_value = 'original'
    os.environ['GOMA_%s' % flag_test_name] = flag_orig_value
    self._module._SetGomaFlagDefaultValueIfEmpty(flag_test_name,
                                                 flag_test_value)
    self.assertEqual(os.environ['GOMA_%s' % flag_test_name], flag_orig_value)

  def testParseManifestContentsShouldReturnEmptyForEmptyLine(self):
    self.assertEqual(self._module._ParseManifestContents(''), {})

  def testParseManifestContentsShouldParseOneLine(self):
    parsed = self._module._ParseManifestContents('key=val')
    self.assertEqual(len(parsed), 1)
    self.assertTrue('key' in parsed)
    self.assertEqual(parsed['key'], 'val')

  def testParseManifestContentsShouldParseMultipleLines(self):
    parsed = self._module._ParseManifestContents('key0=val0\nkey1=val1')
    self.assertEqual(len(parsed), 2)
    self.assertTrue('key0' in parsed)
    self.assertEqual(parsed['key0'], 'val0')
    self.assertTrue('key1' in parsed)
    self.assertEqual(parsed['key1'], 'val1')

  def testParseManifestContentsShouldShowEmptyValueIfEndWithEqual(self):
    parsed = self._module._ParseManifestContents('key=')
    self.assertEqual(len(parsed), 1)
    self.assertTrue('key' in parsed)
    self.assertEqual(parsed['key'], '')

  def testParseManifestContentsShouldParseLineWithMultipleEquals(self):
    parsed = self._module._ParseManifestContents('key=label=value')
    self.assertEqual(len(parsed), 1)
    self.assertTrue('key' in parsed)
    self.assertEqual(parsed['key'], 'label=value')

  def testParseManifestContentsShouldIgnoreLineWitoutEquals(self):
    parsed = self._module._ParseManifestContents('key')
    self.assertEqual(len(parsed), 0)
    self.assertFalse('key' in parsed)

  def testParseTaskList(self):
    test = (
        'Image name      PID  Session Name      Session#    Mem Usage\n'
        '============== ==== ============= ============= ============\n'
        'compiler.exe    123       Console             1     33,000 K\n'
        'system.exe      456        System             2     10,000 K\n'
        )
    expected = [
        {'Image name': 'compiler.exe',
         'PID': '123',
         'Session Name': 'Console',
         'Session#': '1',
         'Mem Usage': '33,000 K'},
        {'Image name': 'system.exe',
         'PID': '456',
         'Session Name': 'System',
         'Session#': '2',
         'Mem Usage': '10,000 K'},
        ]
    parsed = self._module._ParseTaskList(test)
    self.assertEqual(parsed, expected)

  def testParseTaskListShouldRaiseErrorWithEmpty(self):
    self.assertRaises(self._module.Error, self._module._ParseTaskList, '')

  def testParseTaskListShouldRaiseErrorWithElmLenMismatch(self):
    test = (
        'Image name      PID  Session Name      Session#    Mem Usage\n'
        '============== ==== ============= ============= ============\n'
        'compiler.exe    123       Console\n'
    )
    self.assertRaises(self._module.Error, self._module._ParseTaskList, test)

  def testParseTaskListShouldRaiseErrorWithNoSpaceBetweenValues(self):
    test = (
        'Image name      PID  Session Name      Session#    Mem Usage\n'
        '============== ==== ============= ============= ============\n'
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n'
    )
    self.assertRaises(self._module.Error, self._module._ParseTaskList, test)

  def testParseTaskListShouldSkipBlankLines(self):
    test = (
        ' \n'
        'Image name      PID  Session Name      Session#    Mem Usage\n'
        '\r \n'
        '============== ==== ============= ============= ============\n'
        '\v\n'
        'compiler.exe    123       Console             1     33,000 K\n'
        '\t\n'
      )
    expected = [
        {'Image name': 'compiler.exe',
         'PID': '123',
         'Session Name': 'Console',
         'Session#': '1',
         'Mem Usage': '33,000 K'},
        ]
    parsed = self._module._ParseTaskList(test)
    self.assertEqual(parsed, expected)

  def testParseLsofShouldParse(self):
    test = ('u1\n'
            'p2\n'
            'nlocalhost:8088\n'
            'u3\n'
            'p4\n'
            'n/tmp/foo.txt\n')
    expected = [{'uid': 1, 'pid': 2, 'name': 'localhost:8088'},
                {'uid': 3, 'pid': 4, 'name': '/tmp/foo.txt'}]
    parsed = self._module._ParseLsof(test)
    self.assertEqual(parsed, expected)

  def testParseLsofShouldIgnoreUnknown(self):
    test = ('x\n'
            'y\n'
            'u1\n'
            'p2\n'
            'nlocalhost:8088\n')
    expected = [{'uid': 1, 'pid': 2, 'name': 'localhost:8088'}]
    parsed = self._module._ParseLsof(test)
    self.assertEqual(parsed, expected)

  def testParseLsofShouldIgnoreEmptyLine(self):
    test = ('\n'
            '\t\t\n'
            '  \n'
            'u1\n'
            'p2\n'
            'nlocalhost:8088\n')
    expected = [{'uid': 1, 'pid': 2, 'name': 'localhost:8088'}]
    parsed = self._module._ParseLsof(test)
    self.assertEqual(parsed, expected)

  def testParseLsofShouldIgnoreTypeStream(self):
    test = ('u1\n'
            'p2\n'
            'n/tmp/goma.ipc type=STREAM\n')
    expected = [{'uid': 1, 'pid': 2, 'name': '/tmp/goma.ipc'}]
    parsed = self._module._ParseLsof(test)
    self.assertEqual(parsed, expected)

  def testGetEnvMatchedConditionShouldReturnForEmptyCandidates(self):
    default_value = 'default'
    result = self._module._GetEnvMatchedCondition([],
                                                  lambda x: True,
                                                  default_value)
    self.assertEqual(result, default_value)

  def testGetEnvMatchedConditionShouldReturnIfNothingMatched(self):
    default_value = 'default'
    flag_test_name = 'FLAG_TEST'
    flag_value = 'not matched value'
    os.environ['GOMA_%s' % flag_test_name] = flag_value
    result = self._module._GetEnvMatchedCondition(['GOMA_%s' % flag_test_name],
                                                  lambda x: False,
                                                  default_value)
    self.assertEqual(result, default_value)

  def testGetEnvMatchedConditionShouldReturnMatchedValue(self):
    default_value = 'default'
    flag_test_name = 'FLAG_TEST'
    flag_value = 'expected_value'
    os.environ['GOMA_%s' % flag_test_name] = flag_value
    result = self._module._GetEnvMatchedCondition(['GOMA_%s' % flag_test_name],
                                                  lambda x: True,
                                                  default_value)
    self.assertEqual(result, flag_value)

  def testGetEnvMatchedConditionShouldReturnTheFirstCandidate(self):
    default_value = 'default'
    flag_test_name_1 = 'FLAG_TEST_1'
    flag_value_1 = 'value_01'
    os.environ['GOMA_%s' % flag_test_name_1] = flag_value_1
    flag_test_name_2 = 'FLAG_TEST_2'
    flag_value_2 = 'value_02'
    os.environ['GOMA_%s' % flag_test_name_2] = flag_value_2
    result = self._module._GetEnvMatchedCondition(
        ['GOMA_%s' % i for i in [flag_test_name_1, flag_test_name_2]],
        lambda x: True, default_value)
    self.assertEqual(result, flag_value_1)

  def testGetEnvMatchedConditionShouldReturnEarlierCandidateInList(self):
    default_value = 'default'
    flag_name_1 = 'FLAG_TEST_1'
    flag_value_1 = 'value_01'
    os.environ['GOMA_%s' % flag_name_1] = flag_value_1
    flag_name_2 = 'FLAG_TEST_2'
    flag_value_2 = 'match_02'
    os.environ['GOMA_%s' % flag_name_2] = flag_value_2
    flag_name_3 = 'FLAG_TEST_3'
    flag_value_3 = 'match_03'
    os.environ['GOMA_%s' % flag_name_3] = flag_value_3
    result = self._module._GetEnvMatchedCondition(
        ['GOMA_%s' % i for i in [flag_name_1, flag_name_2, flag_name_3]],
        lambda x: x.startswith('match'), default_value)
    self.assertEqual(result, flag_value_2)

  def testParseFlagzShouldParse(self):
    test_data = ('GOMA_COMPILER_PROXY_DAEMON_MODE=true\n'
                 'GOMA_COMPILER_PROXY_LOCK_FILENAME=goma_compiler_proxy.lock\n')
    parsed_data = self._module._ParseFlagz(test_data)
    expected = {
        'GOMA_COMPILER_PROXY_DAEMON_MODE': 'true',
        'GOMA_COMPILER_PROXY_LOCK_FILENAME': 'goma_compiler_proxy.lock',
    }
    self.assertEqual(parsed_data, expected)

  def testParseFlagzShouldIgnoreAutoConfigured(self):
    test_data = ('GOMA_BURST_MAX_SUBPROCS=64 (auto configured)\n'
                 'GOMA_COMPILER_PROXY_LOCK_FILENAME=goma_compiler_proxy.lock\n')
    parsed_data = self._module._ParseFlagz(test_data)
    expected = {
        'GOMA_COMPILER_PROXY_LOCK_FILENAME': 'goma_compiler_proxy.lock',
    }
    self.assertEqual(parsed_data, expected)

  def testParseFlagzShouldIgnoreNewlineOnlyLine(self):
    test_data = ('\n'
                 'GOMA_COMPILER_PROXY_DAEMON_MODE=true\n'
                 '\n\n'
                 '\r\n'
                 'GOMA_COMPILER_PROXY_LOCK_FILENAME=goma_compiler_proxy.lock\n'
                 '\n')
    parsed_data = self._module._ParseFlagz(test_data)
    expected = {
        'GOMA_COMPILER_PROXY_DAEMON_MODE': 'true',
        'GOMA_COMPILER_PROXY_LOCK_FILENAME': 'goma_compiler_proxy.lock',
    }
    self.assertEqual(parsed_data, expected)

  def testParseFlagzShouldIgnoreWhiteSpaces(self):
    test_data = (' \t  GOMA_COMPILER_PROXY_DAEMON_MODE \t = \t true  \n')
    parsed_data = self._module._ParseFlagz(test_data)
    expected = {
        'GOMA_COMPILER_PROXY_DAEMON_MODE': 'true',
    }
    self.assertEqual(parsed_data, expected)

  def testIsGomaFlagUpdatedShouldReturnFalseIfNothingHasSet(self):
    self.assertFalse(self._module._IsGomaFlagUpdated({}))

  def testIsGomaFlagUpdatedShouldReturnTrueIfNewFlag(self):
    os.environ['GOMA_TEST'] = 'test'
    self.assertTrue(self._module._IsGomaFlagUpdated({}))

  def testIsGomaFlagUpdatedShouldReturnTrueIfFlagRemoved(self):
    self.assertTrue(self._module._IsGomaFlagUpdated({'GOMA_TEST': 'test'}))

  def testIsGomaFlagUpdatedShouldReturnFalseIfNoUpdate(self):
    expected = {'GOMA_TEST': 'test'}
    for key, value in expected.items():
      os.environ[key] = value
    self.assertFalse(self._module._IsGomaFlagUpdated(expected))

  def testStartCompilerProxyShouldRun(self):
    driver = self._module.GomaDriver(FakeGomaEnv())
    driver._StartCompilerProxy()

  def testGetStatusShouldCallControlCompilerProxyWithHealthz(self):
    env = self.CreateSpyControlCompilerProxy()
    driver = self._module.GomaDriver(env)
    driver._GetStatus()
    self.assertEqual(env.command, '/healthz')

  def testShutdownCompilerProxyShouldCallControlCompilerProxyWith3Quit(self):
    env = self.CreateSpyControlCompilerProxy()
    driver = self._module.GomaDriver(env)
    driver._ShutdownCompilerProxy()
    self.assertEqual(env.command, '/quitquitquit')

  def testPrintStatisticsShouldCallControlCompilerProxyWithStatz(self):
    env = self.CreateSpyControlCompilerProxy()
    driver = self._module.GomaDriver(env)
    driver._PrintStatistics()
    self.assertEqual(env.command, '/statz')

  def testPrintHistogramShouldCallControlCompilerProxyWithStatz(self):
    env = self.CreateSpyControlCompilerProxy()
    driver = self._module.GomaDriver(env)
    driver._PrintHistogram()
    self.assertEqual(env.command, '/histogramz')

  def testGetJsonStatusShouldCallControlCompilerProxyWithErrorz(self):
    env = self.CreateSpyControlCompilerProxy()
    driver = self._module.GomaDriver(env)
    driver._GetJsonStatus()
    self.assertEqual(env.command, '/errorz')

  def testReportMakeTgz(self):
    class SpyGomaEnv(FakeGomaEnv):
      """Spy GomaEnv to provide WriteFile, CopyFile and MakeTgzFromDirectory"""

      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.output_files = []
        self.tgz_source_dir = None
        self.tgz_file = None
        self.written = False
        self.find_latest_info_file = False

      def WriteFile(self, filename, content):
        self.output_files.append(filename)

      def CopyFile(self, from_file, to_file):
        self.output_files.append(to_file)

      @staticmethod
      def FindLatestLogFile(command_name, log_type):
        return None

      def MakeTgzFromDirectory(self, dir_name, output_filename):
        self.tgz_source_dir = dir_name
        self.tgz_file = output_filename
        self.written = True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._Report()
    self.assertTrue(env.written)
    for f in env.output_files:
      self.assertTrue(f.startswith(env.tgz_source_dir))
    self.assertTrue(env.tgz_file.startswith(self._module._GetTempDirectory()))

  def testReportMakeTgzWithoutCompilerProxyRunning(self):
    class SpyGomaEnv(FakeGomaEnv):
      """Spy GomaEnv to provide WriteFile, CopyFile and MakeTgzFromDirectory.
         Also, compiler_proxy is not running in this env."""

      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.output_files = []
        self.tgz_source_dir = None
        self.tgz_file = None
        self.written = False

      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        if command == '/healthz':
          return {
              'status': False,
              'message': 'compiler proxy is not running',
              'url': 'fake',
              'pid': 'unknown',
          }
        # /compilerz, /histogramz, /serverz, or /statz won't be called.
        if command in ['/compilerz', '/histogramz', '/serverz', '/statz']:
          raise Exception('Unexpected command is called')
        return super(SpyGomaEnv, self).ControlCompilerProxy(command,
                                                            check_running,
                                                            need_pids)

      def WriteFile(self, filename, content):
        self.output_files.append(filename)

      def CopyFile(self, from_file, to_file):
        self.output_files.append(to_file)

      @staticmethod
      def FindLatestLogFile(command_name, log_type):
        return None

      def MakeTgzFromDirectory(self, dir_name, output_filename):
        self.tgz_source_dir = dir_name
        self.tgz_file = output_filename
        self.written = True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._Report()
    self.assertTrue(env.written)
    for f in env.output_files:
      self.assertTrue(f.startswith(env.tgz_source_dir))
    self.assertTrue(env.tgz_file.startswith(self._module._GetTempDirectory()))

  def testReportMakeTgzCompilerProxyDeadAfterHealthz(self):
    class SpyGomaEnv(FakeGomaEnv):
      """Spy GomaEnv to provide WriteFile, CopyFile and MakeTgzFromDirectory.
         compiler_proxy dies after the first /healthz call."""

      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.output_files = []
        self.tgz_source_dir = None
        self.tgz_file = None
        self.written = False
        self.is_dead = False

      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        if self.is_dead:
          return {
              'status': False,
              'message': 'compiler_proxy is not running',
              'url': 'dummy',
              'pid': 'unknown',
          }
        # Die after /healthz is called. The first /healthz should be
        # processed correctly.
        if command == '/healthz':
          self.is_dead = True
        return super(SpyGomaEnv, self).ControlCompilerProxy(command,
                                                            check_running,
                                                            need_pids)

      def WriteFile(self, filename, content):
        self.output_files.append(filename)

      def CopyFile(self, from_file, to_file):
        self.output_files.append(to_file)

      @staticmethod
      def FindLatestLogFile(command_name, log_type):
        return None

      def MakeTgzFromDirectory(self, dir_name, output_filename):
        self.tgz_source_dir = dir_name
        self.tgz_file = output_filename
        self.written = True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._Report()
    self.assertTrue(env.written)
    for f in env.output_files:
      self.assertTrue(f.startswith(env.tgz_source_dir))
    self.assertTrue(env.tgz_file.startswith(self._module._GetTempDirectory()))

  def testRestartCompilerProxyShouldRun(self):
    driver = self._module.GomaDriver(FakeGomaEnv())
    driver._RestartCompilerProxy()

  def testGomaStatusShouldTrueForOK(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.compiler_proxy_healthz_called = False
      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        if command == '/healthz':
          self.compiler_proxy_healthz_called = True
          return {'status': True, 'message': 'ok', 'url': 'fake',
                  'pid': 'unknown'}
        return super(SpyGomaEnv, self).ControlCompilerProxy(command,
                                                            check_running,
                                                            need_pids)

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    self.assertTrue(driver._GetStatus())
    self.assertTrue(env.compiler_proxy_healthz_called)

  def testGomaStatusShouldTrueForRunning(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.compiler_proxy_healthz_called = False
      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        if command == '/healthz':
          self.compiler_proxy_healthz_called = True
          return {'status': True, 'message': 'running: had some error',
                  'url': 'fake', 'pid': 'unknown'}
        return super(SpyGomaEnv, self).ControlCompilerProxy(command,
                                                            check_running,
                                                            need_pids)

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    self.assertTrue(driver._GetStatus())
    self.assertTrue(env.compiler_proxy_healthz_called)

  def testGomaStatusShouldFalseForError(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.compiler_proxy_healthz_called = False
      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        if command == '/healthz':
          self.compiler_proxy_healthz_called = True
          return {'status': True, 'message': 'error: had some error',
                  'url': 'fake', 'pid': 'unknown'}
        return super(SpyGomaEnv, self).ControlCompilerProxy(command,
                                                            check_running,
                                                            need_pids)

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    self.assertFalse(driver._GetStatus())
    self.assertTrue(env.compiler_proxy_healthz_called)

  def testGomaStatusShouldFalseForUnresponseHealthz(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.compiler_proxy_healthz_called = False
      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        if command == '/healthz':
          self.compiler_proxy_healthz_called = True
          return {'status': False, 'message': '', 'url': 'fake',
                  'pid': 'unknown'}
        return super(SpyGomaEnv, self).ControlCompilerProxy(command,
                                                            check_running,
                                                            need_pids)

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    self.assertFalse(driver._GetStatus())
    self.assertTrue(env.compiler_proxy_healthz_called)

  def testEnsureStartShouldRestartCompilerProxyIfBinaryHasUpdated(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.compiler_proxy_running = False
        self.control_with_quit = False
        self.control_with_health = False
        self.control_with_version = False
        self.get_version = False
        self.kill_stakeholders = False

      def CompilerProxyRunning(self):
        self.compiler_proxy_running = True
        return True

      def GetCompilerProxyVersion(self):
        self.get_version = True
        return 'GOMA version This version should not be matched.'

      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        if command == '/quitquitquit':
          self.control_with_quit = True
        elif command == '/healthz':
          self.control_with_health = True
        elif command == '/versionz':
          self.control_with_version = True
        elif command == '/flagz':
          return {'status': True, 'message': '', 'pid': 'unknown'}
        else:
          raise Exception('Unknown command given.')
        return super(SpyGomaEnv, self).ControlCompilerProxy(command,
                                                            check_running,
                                                            need_pids)

      def KillStakeholders(self, force=False):
        self.kill_stakeholders = True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._EnsureStartCompilerProxy()
    self.assertTrue(env.compiler_proxy_running)
    self.assertTrue(env.get_version)
    self.assertTrue(env.control_with_health)
    self.assertTrue(env.control_with_version)
    self.assertTrue(env.control_with_quit)
    self.assertTrue(env.kill_stakeholders)

  def testEnsureStartShouldRestartCompilerProxyIfHealthzFailed(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.compiler_proxy_running = False
        self.control_with_quit = False
        self.control_with_health = False
        self.control_with_version = False
        self.get_version = False
        self.kill_stakeholders = False
        self.exec_compiler_proxy = False
        self.status_compiler_proxy_running = True
        self.status_healthy = False

      def CompilerProxyRunning(self):
        self.compiler_proxy_running = True
        return self.status_compiler_proxy_running

      def GetCompilerProxyVersion(self):
        self.get_version = True
        return 'GOMA version dummy_version'

      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        if command == '/quitquitquit':
          self.control_with_quit = True
        elif command == '/healthz':
          self.control_with_health = True
          if self.status_healthy:
            return {'status': True,
                    'message': 'ok',
                    'url': 'dummy',
                    'pid': 'unknown',
                    }
          else:
            return {'status': False,
                    'message': 'connect failed',
                    'url': '',
                    'pid': 'unknown',
                    }
        elif command == '/versionz':
          self.control_with_version = True
          return {'status': True, 'message': 'dummy_version', 'url': 'fake'}
        elif command == '/flagz':
          return {'status': True, 'message': ''}
        else:
          raise Exception('Unknown command given.')
        return super(SpyGomaEnv, self).ControlCompilerProxy(command,
                                                            check_running,
                                                            need_pids)

      def KillStakeholders(self, force=False):
        self.kill_stakeholders = True
        self.status_compiler_proxy_running = False

      def ExecCompilerProxy(self):
        self.exec_compiler_proxy = True
        self.status_compiler_proxy_running = True
        self.status_healthy = True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._EnsureStartCompilerProxy()
    self.assertTrue(env.compiler_proxy_running)
    self.assertTrue(env.get_version)
    self.assertTrue(env.control_with_health)
    self.assertTrue(env.control_with_version)
    self.assertTrue(env.control_with_quit)
    self.assertTrue(env.exec_compiler_proxy)
    self.assertTrue(env.kill_stakeholders)

  def testEnsureStartShouldRestartIfFlagsAreChanged(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.compiler_proxy_running = False
        self.control_with_quit = False
        self.control_with_health = False
        self.control_with_version = False
        self.control_with_flagz = False
        self.get_version = False
        self.kill_stakeholders = False

      def CompilerProxyRunning(self):
        self.compiler_proxy_running = True
        return True

      def GetCompilerProxyVersion(self):
        self.get_version = True
        return 'GOMA version dummy_version'

      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        if command == '/quitquitquit':
          self.control_with_quit = True
        elif command == '/healthz':
          self.control_with_health = True
          return {'status': True,
                  'message': 'ok',
                  'url': 'dummy',
                  'pid': 'unknown',
                  }
        elif command == '/versionz':
          self.control_with_version = True
          return {'status': True, 'message': 'dummy_version', 'url': 'fake',
                  'pid': 'unknown'}
        elif command == '/flagz':
          self.control_with_flagz = True
          return {'status': True, 'message': '', 'pid': 'unknown'}
        else:
          raise Exception('Unknown command given.')
        return super(SpyGomaEnv, self).ControlCompilerProxy(command,
                                                            check_running,
                                                            need_pids)

      def KillStakeholders(self, force=False):
        self.kill_stakeholders = True

    os.environ['GOMA_TEST'] = 'flag should be different'
    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._EnsureStartCompilerProxy()
    self.assertTrue(env.compiler_proxy_running)
    self.assertTrue(env.get_version)
    self.assertTrue(env.control_with_health)
    self.assertTrue(env.control_with_version)
    self.assertTrue(env.control_with_quit)
    self.assertTrue(env.control_with_flagz)
    self.assertTrue(env.kill_stakeholders)

  def testEnsureStartShouldRestartIfFlagsAreRemoved(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.compiler_proxy_running = False
        self.control_with_quit = False
        self.control_with_health = False
        self.control_with_version = False
        self.control_with_flagz = False
        self.get_version = False
        self.kill_stakeholders = False

      def CompilerProxyRunning(self):
        self.compiler_proxy_running = True
        return True

      def GetCompilerProxyVersion(self):
        self.get_version = True
        return 'GOMA version dummy_version'

      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        if command == '/quitquitquit':
          self.control_with_quit = True
        elif command == '/healthz':
          self.control_with_health = True
          return {'status': True,
                  'message': 'ok',
                  'url': 'dummy',
                  'pid': 'unknown',
                  }
        elif command == '/versionz':
          self.control_with_version = True
          return {'status': True, 'message': 'dummy_version', 'url': 'fake',
                  'pid': 'unknown'}
        elif command == '/flagz':
          self.control_with_flagz = True
          return {'status': True, 'message': 'GOMA_TEST=test\n',
                  'pid': 'unknown'}
        else:
          raise Exception('Unknown command given.')
        return super(SpyGomaEnv, self).ControlCompilerProxy(command,
                                                            check_running,
                                                            need_pids)

      def KillStakeholders(self, force=False):
        self.kill_stakeholders = True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._EnsureStartCompilerProxy()
    self.assertTrue(env.compiler_proxy_running)
    self.assertTrue(env.get_version)
    self.assertTrue(env.control_with_health)
    self.assertTrue(env.control_with_version)
    self.assertTrue(env.control_with_quit)
    self.assertTrue(env.control_with_flagz)
    self.assertTrue(env.kill_stakeholders)

  def testEnsureStartShouldNotRestartIfFlagsNotChanged(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.compiler_proxy_running = False
        self.control_with_quit = False
        self.control_with_health = False
        self.control_with_version = False
        self.control_with_flagz = False
        self.get_version = False
        self.kill_stakeholders = False

      def CompilerProxyRunning(self):
        self.compiler_proxy_running = True
        return True

      def GetCompilerProxyVersion(self):
        self.get_version = True
        return 'GOMA version dummy_version'

      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        if command == '/quitquitquit':
          self.control_with_quit = True
        elif command == '/healthz':
          self.control_with_health = True
          return {'status': True,
                  'message': 'ok',
                  'url': 'dummy',
                  'pid': 'unknown',
                  }
        elif command == '/versionz':
          self.control_with_version = True
          return {'status': True, 'message': 'dummy_version', 'url': 'fake',
                  'pid': 'unknown'}
        elif command == '/flagz':
          self.control_with_flagz = True
          return {'status': True, 'message': '', 'pid': 'unknown'}
        else:
          raise Exception('Unknown command given.')
        return super(SpyGomaEnv, self).ControlCompilerProxy(command,
                                                            check_running,
                                                            need_pids)

      def KillStakeholders(self, force=False):
        self.kill_stakeholders = True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._EnsureStartCompilerProxy()
    self.assertTrue(env.compiler_proxy_running)
    self.assertTrue(env.get_version)
    self.assertTrue(env.control_with_health)
    self.assertTrue(env.control_with_version)
    self.assertTrue(env.control_with_flagz)
    self.assertFalse(env.control_with_quit)
    self.assertFalse(env.kill_stakeholders)

  def testEnsureStartShouldRestartCompilerProxyIfUnhealthy(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.compiler_proxy_running = False
        self.control_with_quit = False
        self.control_with_health = False
        self.control_with_version = False
        self.get_version = False
        self.kill_stakeholders = False

      def CompilerProxyRunning(self):
        self.compiler_proxy_running = True
        return True

      def GetCompilerProxyVersion(self):
        self.get_version = True
        return 'GOMA version dummy_version'

      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        if command == '/quitquitquit':
          self.control_with_quit = True
        elif command == '/healthz':
          self.control_with_health = True
          return {'status': True,
                  'message': 'running: failed to connect to backend servers',
                  'url': '',
                  'pid': 'unknown',
                  }
        elif command == '/versionz':
          self.control_with_version = True
          return {'status': True, 'message': 'dummy_version', 'url': 'fake',
                  'pid': 'unknown'}
        elif command == '/flagz':
          return {'status': True, 'message': '', 'pid': 'unknown'}
        else:
          raise Exception('Unknown command given.')
        return super(SpyGomaEnv, self).ControlCompilerProxy(command,
                                                            check_running,
                                                            need_pids)

      def KillStakeholders(self, force=False):
        self.kill_stakeholders = True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._EnsureStartCompilerProxy()
    self.assertTrue(env.compiler_proxy_running)
    self.assertTrue(env.get_version)
    self.assertTrue(env.control_with_health)
    self.assertTrue(env.control_with_version)
    self.assertTrue(env.control_with_quit)
    self.assertTrue(env.kill_stakeholders)

  def testIsCompilerProxySilentlyUpdatedShouldReturnTrueIfVersionMismatch(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.get_version = False
        self.control_with_version = False

      def GetCompilerProxyVersion(self):
        self.get_version = True
        return 'GOMA version fake0'

      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        self.control_with_version = True
        if command == '/versionz':
          return {'status': True, 'message': 'fake1', 'url': 'fake',
                  'pid': 'unknown'}

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    self.assertTrue(driver._IsCompilerProxySilentlyUpdated())
    self.assertTrue(env.get_version)
    self.assertTrue(env.control_with_version)

  def testIsCompilerProxySilentlyUpdatedShouldReturnFalseIfVersionMatch(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.get_version = False
        self.control_with_version = False

      def GetCompilerProxyVersion(self):
        self.get_version = True
        return 'GOMA version fake0'

      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        self.control_with_version = True
        if command == '/versionz':
          return {'status': True, 'message': 'fake0', 'url': 'fake',
                  'pid': 'unknown'}

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    self.assertFalse(driver._IsCompilerProxySilentlyUpdated())
    self.assertTrue(env.get_version)
    self.assertTrue(env.control_with_version)

  def testGetJsonStatusShouldShowErrorStatusOnControlCompilerProxyError(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.control_compiler_proxy = False

      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        self.control_compiler_proxy = True
        if command == '/errorz':
          return {'status': False, 'pid': 'unknown'}

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    printed_json = json.loads(driver._GetJsonStatus())
    self.assertTrue(env.control_compiler_proxy)
    self.assertEqual(printed_json['notice'][0]['compile_error'],
                     'COMPILER_PROXY_UNREACHABLE')

  def testGetJsonStatusShouldShowCompilerProxyReplyAsIsIfAvailable(self):
    compiler_proxy_output = '{"fake": 0}'
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.control_compiler_proxy = False

      def ControlCompilerProxy(self, command, check_running=False,
                               need_pids=False):
        self.control_compiler_proxy = True
        if command == '/errorz':
          return {'status': True, 'message': compiler_proxy_output,
                  'pid': 'unknown'}

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    returned = driver._GetJsonStatus()
    self.assertTrue(env.control_compiler_proxy)
    self.assertEqual(returned, compiler_proxy_output)

  def testCreateGomaTmpDirectoryNew(self):
    fake_tmpdir = '/tmp/gomatest_chrome-bot'
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.is_directory_exist = None
        self.make_directory = None
        self.ensure_directory_owned_by_user = None

      def GetGomaTmpDir(self):
        return fake_tmpdir

      def IsDirectoryExist(self, dirname):
        self.is_directory_exist = dirname
        return False

      def MakeDirectory(self, dirname):
        self.make_directory = dirname

      def EnsureDirectoryOwnedByUser(self, dirname):
        self.ensure_directory_owned_by_user = dirname
        return True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    orig_goma_tmp_dir = os.environ.get('GOMA_TMP_DIR')
    self.assertNotEqual(orig_goma_tmp_dir, fake_tmpdir)
    driver._CreateGomaTmpDirectory()
    goma_tmp_dir = os.environ.get('GOMA_TMP_DIR')
    if orig_goma_tmp_dir:
      os.environ['GOMA_TMP_DIR'] = orig_goma_tmp_dir
    else:
      del os.environ['GOMA_TMP_DIR']
    self.assertEqual(env.is_directory_exist, fake_tmpdir)
    self.assertEqual(env.make_directory, fake_tmpdir)
    self.assertEqual(env.ensure_directory_owned_by_user, None)
    self.assertEqual(goma_tmp_dir, fake_tmpdir)

  def testCreateGomaTmpDirectoryExists(self):
    fake_tmpdir = '/tmp/gomatest_chrome-bot'
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.is_directory_exist = None
        self.make_directory = None
        self.ensure_directory_owned_by_user = None

      def GetGomaTmpDir(self):
        return fake_tmpdir

      def IsDirectoryExist(self, dirname):
        self.is_directory_exist = dirname
        return True

      def MakeDirectory(self, dirname):
        self.make_directory = dirname

      def EnsureDirectoryOwnedByUser(self, dirname):
        self.ensure_directory_owned_by_user = dirname
        return True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    orig_goma_tmp_dir = os.environ.get('GOMA_TMP_DIR')
    self.assertNotEqual(orig_goma_tmp_dir, fake_tmpdir)
    driver._CreateGomaTmpDirectory()
    goma_tmp_dir = os.environ.get('GOMA_TMP_DIR')
    if orig_goma_tmp_dir:
      os.environ['GOMA_TMP_DIR'] = orig_goma_tmp_dir
    else:
      del os.environ['GOMA_TMP_DIR']
    self.assertEqual(env.is_directory_exist, fake_tmpdir)
    self.assertEqual(env.make_directory, None)
    self.assertEqual(env.ensure_directory_owned_by_user, fake_tmpdir)
    self.assertEqual(goma_tmp_dir, fake_tmpdir)


  def testCreateCrashDumpDirectoryShouldNotCreateDirectoryIfExist(self):
    fake_dump_dir = '/dump_dir'
    expected_dump_dir = fake_dump_dir

    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.is_directory_exist = False
        self.is_directory_exist_dir = ''
        self.ensure_directory_owned_by_user = False
        self.ensure_directory_owned_by_user_dir = ''
        self.make_directory = False
        self.get_crash_dump_directory = False

      def GetCrashDumpDirectory(self):
        self.get_crash_dump_directory = True
        return fake_dump_dir

      def IsDirectoryExist(self, dirname):
        self.is_directory_exist = True
        self.is_directory_exist_dir = dirname
        return True

      def EnsureDirectoryOwnedByUser(self, dirname):
        self.ensure_directory_owned_by_user = True
        self.ensure_directory_owned_by_user_dir = dirname
        return True

      def MakeDirectory(self, _):
        self.make_directory = True
        return True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._CreateCrashDumpDirectory()
    self.assertTrue(env.get_crash_dump_directory)
    self.assertTrue(env.is_directory_exist)
    self.assertEqual(env.is_directory_exist_dir, expected_dump_dir)
    self.assertTrue(env.ensure_directory_owned_by_user)
    self.assertEqual(env.ensure_directory_owned_by_user_dir, expected_dump_dir)
    self.assertFalse(env.make_directory)

  def testCreateCrashDumpDirectoryShouldCreateDirectoryIfNotExist(self):
    fake_dump_dir = '/dump_dir'
    expected_dump_dir = fake_dump_dir

    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.is_directory_exist = False
        self.is_directory_exist_dir = ''
        self.ensure_directory_owned_by_user = False
        self.make_directory = False
        self.make_directory_dir = ''
        self.get_crash_dump_directory = False

      def GetCrashDumpDirectory(self):
        self.get_crash_dump_directory = True
        return fake_dump_dir

      def IsDirectoryExist(self, dirname):
        self.is_directory_exist = True
        self.is_directory_exist_dir = dirname
        return False

      def EnsureDirectoryOwnedByUser(self, _):
        self.ensure_directory_owned_by_user = True
        return True

      def MakeDirectory(self, dirname):
        self.make_directory = True
        self.make_directory_dir = dirname
        return True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._CreateCrashDumpDirectory()
    self.assertTrue(env.get_crash_dump_directory)
    self.assertTrue(env.is_directory_exist)
    self.assertEqual(env.is_directory_exist_dir, expected_dump_dir)
    self.assertFalse(env.ensure_directory_owned_by_user)
    self.assertTrue(env.make_directory)
    self.assertEqual(env.make_directory_dir, expected_dump_dir)

  def testCreateCacheDirectoryShouldNotCreateDirectoryIfExist(self):
    fake_cache_dir = '/cache_dir'
    expected_cache_dir = fake_cache_dir

    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.is_directory_exist = False
        self.is_directory_exist_dir = ''
        self.ensure_directory_owned_by_user = False
        self.ensure_directory_owned_by_user_dir = ''
        self.make_directory = False
        self.get_cache_directory = False

      def GetCacheDirectory(self):
        self.get_cache_directory = True
        return fake_cache_dir

      def IsDirectoryExist(self, dirname):
        self.is_directory_exist = True
        self.is_directory_exist_dir = dirname
        return True

      def EnsureDirectoryOwnedByUser(self, dirname):
        self.ensure_directory_owned_by_user = True
        self.ensure_directory_owned_by_user_dir = dirname
        return True

      def MakeDirectory(self, _):
        self.make_directory = True
        return True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._CreateCacheDirectory()
    self.assertTrue(env.get_cache_directory)
    self.assertTrue(env.is_directory_exist)
    self.assertEqual(env.is_directory_exist_dir, expected_cache_dir)
    self.assertTrue(env.ensure_directory_owned_by_user)
    self.assertEqual(env.ensure_directory_owned_by_user_dir,
                     expected_cache_dir)
    self.assertFalse(env.make_directory)

  def testCreateCacheDirectoryShouldCreateDirectoryIfNotExist(self):
    fake_cache_dir = 'cache_dir'
    expected_cache_dir = fake_cache_dir

    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.is_directory_exist = False
        self.is_directory_exist_dir = ''
        self.ensure_directory_owned_by_user = False
        self.make_directory = False
        self.make_directory_dir = ''
        self.get_cache_directory = False

      def GetCacheDirectory(self):
        self.get_cache_directory = True
        return fake_cache_dir

      def IsDirectoryExist(self, dirname):
        self.is_directory_exist = True
        self.is_directory_exist_dir = dirname
        return False

      def EnsureDirectoryOwnedByUser(self, _):
        self.ensure_directory_owned_by_user = True
        return True

      def MakeDirectory(self, dirname):
        self.make_directory = True
        self.make_directory_dir = dirname
        return True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._CreateCacheDirectory()
    self.assertTrue(env.get_cache_directory)
    self.assertTrue(env.is_directory_exist)
    self.assertEqual(env.is_directory_exist_dir, expected_cache_dir)
    self.assertFalse(env.ensure_directory_owned_by_user)
    self.assertTrue(env.make_directory)

  def testAuditShouldReturnTrueForEmptyJSON(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.load_checksum = False
        self.calculate_checksum = False

      def LoadChecksum(self):
        self.load_checksum = True
        return {}

      def CalculateChecksum(self, _):
        self.calculate_checksum = True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    self.assertTrue(driver._Audit())
    self.assertTrue(env.load_checksum)
    self.assertFalse(env.calculate_checksum)

  def testAuditShouldReturnTrueForValidChecksum(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.load_checksum = False
        self.calculate_checksum = False

      def LoadChecksum(self):
        self.load_checksum = True
        return {'compiler_proxy': 'valid_checksum'}

      def CalculateChecksum(self, filename):
        self.calculate_checksum = True
        assert filename == 'compiler_proxy'
        return 'valid_checksum'

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    self.assertTrue(driver._Audit())
    self.assertTrue(env.load_checksum)
    self.assertTrue(env.calculate_checksum)

  def testAuditShouldReturnFalseForInvalidChecksum(self):
    class SpyGomaEnv(FakeGomaEnv):
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.load_checksum = False
        self.calculate_checksum = False

      def LoadChecksum(self):
        self.load_checksum = True
        return {'compiler_proxy': 'valid_checksum'}

      def CalculateChecksum(self, filename):
        self.calculate_checksum = True
        assert filename == 'compiler_proxy'
        return 'invalid_checksum'

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    self.assertFalse(driver._Audit())
    self.assertTrue(env.load_checksum)
    self.assertTrue(env.calculate_checksum)


class GomaEnvTest(GomaCtlTestCommon):
  """Medium tests for GomaEnv in goma_ctl.py.

  Some tests in this class may affect external environment.
  """
  # test should be able to access protected members and variables.
  # pylint: disable=W0212

  def testShouldSetPlatformEnvIfPlatformNotInManifest(self):
    os.environ['PLATFORM'] = 'goobuntu'
    self.assertTrue(os.environ.get('PLATFORM'))
    env = self._module.GomaEnv()
    self.assertFalse(os.path.exists(os.path.join(env._dir, 'MANIFEST')))
    self.assertEqual(env._platform, 'goobuntu')

  def testShouldPreferPlatformInManifestToEnv(self):
    os.environ['PLATFORM'] = 'goobuntu'
    self.assertTrue(os.environ.get('PLATFORM'))
    manifest_file = os.path.join(self._tmp_dir, self._TMP_SUBDIR_NAME,
                                 'MANIFEST')
    with open(manifest_file, 'w') as f:
      f.write('PLATFORM=linux')
    env = self._module.GomaEnv()
    self.assertTrue(os.path.exists(os.path.join(env._dir, 'MANIFEST')))
    self.assertEqual(env._platform, 'linux')

  def testGeneratedChecksumShouldBeValid(self):
    env = self._module.GomaEnv()
    cksums = env.LoadChecksum()
    self.assertTrue(cksums)
    for filename, checksum in cksums.items():
      self.assertEqual(env.CalculateChecksum(filename), checksum)

  def testIsOldFileShouldReturnTrueForOldFile(self):
    filename = os.path.join(self._tmp_dir, self._TMP_SUBDIR_NAME, 'test')
    with open(filename, 'w') as f:
      f.write('test')
    env = self._module.GomaEnv()
    env._time = time.time() + 120
    os.environ['GOMA_LOG_CLEAN_INTERVAL'] = '1'
    self.assertTrue(env.IsOldFile(filename))

  def testIsOldFileShouldReturnFalseIfAFileIsNew(self):
    filename = os.path.join(self._tmp_dir, self._TMP_SUBDIR_NAME, 'test')
    with open(filename, 'w') as f:
      f.write('test')
    env = self._module.GomaEnv()
    env._time = time.time()
    os.environ['GOMA_LOG_CLEAN_INTERVAL'] = '60'
    self.assertFalse(env.IsOldFile(filename))

  def testIsOldFileShouldReturnFalseIfLogCleanIntervalIsNegative(self):
    filename = os.path.join(self._tmp_dir, self._TMP_SUBDIR_NAME, 'test')
    with open(filename, 'w') as f:
      f.write('test')
    env = self._module.GomaEnv()
    env._time = time.time() + 120
    os.environ['GOMA_LOG_CLEAN_INTERVAL'] = '-1'
    self.assertFalse(env.IsOldFile(filename))

  def testMakeDirectory(self):
    env = self._module._GOMA_ENVS[os.name]()
    tmpdir = tempfile.mkdtemp()
    os.rmdir(tmpdir)
    self.assertFalse(os.path.exists(tmpdir))
    env.MakeDirectory(tmpdir)
    self.assertTrue(os.path.isdir(tmpdir))
    if os.name != 'nt':
      st = os.stat(tmpdir)
      self.assertEqual(st.st_uid, os.geteuid())
      self.assertEqual((st.st_mode & 0o77), 0)
    os.rmdir(tmpdir)

  def testEnsureDirectoryOwnedByUser(self):
    tmpdir = tempfile.mkdtemp()
    env = self._module._GOMA_ENVS[os.name]()
    if os.name == 'nt':
      self.assertTrue(env.EnsureDirectoryOwnedByUser(tmpdir))
      os.rmdir(tmpdir)
      return
    self._module._GetPlatformSpecificTempDirectory = lambda: None
    # test only permissions will not have readable/writable for group/other.
    os.chmod(tmpdir, 0o755)
    st = os.stat(tmpdir)
    self.assertEqual(st.st_uid, os.geteuid())
    self.assertNotEqual((st.st_mode & 0o77), 0)
    self.assertTrue(env.EnsureDirectoryOwnedByUser(tmpdir))
    self.assertTrue(os.path.isdir(tmpdir))
    st = os.stat(tmpdir)
    self.assertEqual(st.st_uid, os.geteuid())
    self.assertEqual((st.st_mode & 0o77), 0)
    os.rmdir(tmpdir)

  def testCreateCacheDirectoryShouldUseDefaultIfNoEnv(self):
    fake_tmp_dir = '/fake_tmp'
    expected_cache_dir = os.path.join(
        fake_tmp_dir, self._module._CACHE_DIR)

    env = self._module._GOMA_ENVS[os.name]()
    env.GetGomaTmpDir = lambda: fake_tmp_dir
    self.assertEqual(env.GetCacheDirectory(), expected_cache_dir)

  def testCreateCacheDirectoryShouldRespectCacheDirEnv(self):
    fake_tmp_dir = '/fake_tmp'
    fake_cache_dir = '/fake_cache_dir'
    expected_cache_dir = fake_cache_dir

    env = self._module._GOMA_ENVS[os.name]()
    env.GetGomaTmpDir = lambda: fake_tmp_dir
    try:
      backup = os.environ.get('GOMA_CACHE_DIR')
      os.environ['GOMA_CACHE_DIR'] = fake_cache_dir
      self.assertEqual(env.GetCacheDirectory(), expected_cache_dir)
    finally:
      if backup:
        os.environ['GOMA_CACHE_DIR'] = backup
      else:
        del os.environ['GOMA_CACHE_DIR']

  def testShouldFallbackGomaUsernameNoEnvIfNoEnvSet(self):
    self._module._GetUsernameEnv = lambda: ''
    env = self._module._GOMA_ENVS[os.name]()
    self.assertNotEqual(env.GetUsername(), '')


class GomaCtlLargeTest(GomaCtlTestCommon):
  """Large tests for goma_ctl.py.

  All tests in this class may affect external environment.  It may try to
  download packages from servers and I/O local files in test environment.
  """
  # test should be able to access protected members and variables.
  # pylint: disable=W0212

  def __init__(self, method_name, goma_ctl_path, platform_specific,
               oauth2_file, port):
    """Initialize GomaCtlTest.

    Args:
      method_name: a string of test method name to execute.
      goma_ctl_path: a string of goma directory name.
      platform_specific: a object for providing platform specific behavior.
      oauth2_file: a string of OAuth2 service account JSON filename.
      port: a string or an integer port number of compiler_proxy.
    """
    super(GomaCtlLargeTest, self).__init__(method_name, goma_ctl_path,
                                           platform_specific)
    self._oauth2_file = oauth2_file
    self._port = int(port)
    self._driver = None

  def setUp(self):
    super(GomaCtlLargeTest, self).setUp()
    self._platform_specific.SetCompilerProxyEnv(self._tmp_dir, self._port)

    os.environ['GOMA_SERVICE_ACCOUNT_JSON_FILE'] = self._oauth2_file
    sys.stderr.write('Using GOMA_SERVICE_ACCOUNT_JSON_FILE = %s\n' %
                     self._oauth2_file)

  def tearDown(self):
    if self._driver:
      self._driver._EnsureStopCompilerProxy()
    super(GomaCtlLargeTest, self).tearDown()

  def StartWithModifiedVersion(self, version=None):
    """Start compiler proxy with modified version.

    Since start-up method is overwritten with dummy method, we do not need
    to stop the compiler proxy.

    Args:
      version: current version to be written.
    """
    driver = self._module.GetGomaDriver()
    manifest = {}
    if version:
      manifest['VERSION'] = version
      # Not goma_ctl to ask the platform, let me put 'PLATFORM' param here.
      manifest['PLATFORM'] = self._platform_specific.GetPlatform()
      driver._env.WriteManifest(manifest)
    driver = self._module.GetGomaDriver()
    # Put fake methods instead of actual one to improve performance of tests.
    driver._env.GetCompilerProxyVersion = lambda dummy = None: 'dummy'
    driver._env.ExecCompilerProxy = lambda dummy = None: True
    def DummyControlCompilerProxy(dummy, **_):
      return {'status': True, 'message': 'msg', 'url': 'url', 'pid': '1'}
    driver._env.ControlCompilerProxy = DummyControlCompilerProxy
    driver._env.CompilerProxyRunning = lambda dummy = None: True
    driver._StartCompilerProxy()

  def testEnsureShouldWorkWithoutFuserCommand(self):
    if isinstance(self._platform_specific, WindowsSpecific):
      return  # Windows don't need this test.

    self._driver = self._module.GetGomaDriver()
    self._driver._env._platform = self._platform_specific.GetPlatform()
    if not self._driver._env._GetFuserPath():
      return  # No need to run this test.
    self._driver._env._GetFuserPath = lambda dummy = None: ''
    try:
      self.assertFalse(self._driver._env.CompilerProxyRunning())
      self._driver._EnsureStartCompilerProxy()
      self.assertTrue(self._driver._env.CompilerProxyRunning())
    finally:
      self._driver._EnsureStopCompilerProxy()

  def testMultipleCompilerProxyInstancesRuns(self):
    if isinstance(self._platform_specific, WindowsSpecific):
      return  # Windows don't support this feature.

    self._driver = self._module.GetGomaDriver()
    self._driver._env._platform = self._platform_specific.GetPlatform()
    try:
      self.assertFalse(self._driver._env.CompilerProxyRunning())
      self._driver._EnsureStartCompilerProxy()
      self.assertTrue(self._driver._env.CompilerProxyRunning())

      prev_envs = {}
      try:
        envs = [
            'GOMA_COMPILER_PROXY_PORT',
            'GOMA_COMPILER_PROXY_SOCKET_NAME',
            'GOMA_COMPILER_PROXY_LOCK_FILENAME']
        for env in envs:
          prev_envs[env] = os.environ.get(env)

        os.environ['GOMA_COMPILER_PROXY_PORT'] = str(int(self._port) + 1)
        os.environ['GOMA_COMPILER_PROXY_SOCKET_NAME'] = 'goma.ipc_test'
        os.environ['GOMA_COMPILER_PROXY_LOCK_FILENAME'] = (
            '/tmp/goma_compiler_proxy.lock.test')
        self.assertFalse(self._driver._env.CompilerProxyRunning())
        self._driver._EnsureStartCompilerProxy()
        self.assertTrue(self._driver._env.CompilerProxyRunning())
      finally:
        self._driver._EnsureStopCompilerProxy()
        for key, value in prev_envs.items():
          if value:
            os.environ[key] = value

    finally:
      self._driver._EnsureStopCompilerProxy()

  def testVersionNotRunning(self):
    class SpyGomaEnv(FakeGomaEnv):
      """Spy GomaEnv for not running compiler_proxy"""
      def __init__(self):
        super(SpyGomaEnv, self).__init__()

      def GetCompilerProxyVersion(self):
        return 'GOMA version versionhash@time'

      def CompilerProxyBinary(self):
        return '/path/to/compiler_proxy'

      def ControlCompilerProxy(self, command, check_running=True,
                               need_pids=False):
        return {'status': False, 'message': 'goma is not running.', 'url': ''}

      def CompilerProxyRunning(self):
        return False

    driver = self._module.GomaDriver(SpyGomaEnv())
    driver._PrintVersion()

  def testVersionRunning(self):
    class SpyGomaEnv(FakeGomaEnv):
      """Spy GomaEnv for running compiler_proxy"""
      def __init__(self):
        super(SpyGomaEnv, self).__init__()

      def GetCompilerProxyVersion(self):
        return 'GOMA version versionhash@time'

      def CompilerProxyBinary(self):
        return '/path/to/compiler_proxy'

      def ControlCompilerProxy(self, command, check_running=True,
                               need_pids=False):
        if command == '/versionz':
          return {'status': True, 'message': 'versionhash@time', 'url': ''}
        return super(SpyGomaEnv, self).ControlCompilerProxy(command,
                                                            check_running,
                                                            need_pids)

    driver = self._module.GomaDriver(SpyGomaEnv())
    driver._PrintVersion()

  def testUpdateHookNotRunning(self):
    class SpyGomaEnv(FakeGomaEnv):
      """Spy GomaEnv for update check"""
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.shutdowned = False
        self.running = False

      def GetCompilerProxyVersion(self):
        return 'GOMA version versionhash@time'

      def ControlCompilerProxy(self, command, check_running=True,
                               need_pids=False):
        if command == '/quitquitquit' and self.running:
          self.shutdowned = True
          self.running = False
          return {'status': False, 'message': 'ok', 'url': '', 'pid': '1234'}
        if command == '/versionz' and self.running:
          return {
              'status': True,
              'message': 'versionhash@time',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        if command == '/progz' and self.running:
          return {
              'status': True,
              'message': '/path/to/compiler_proxy',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        if self.running:
          return {
              'status': True,
              'message': 'ok',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        return {
            'status': False,
            'message': 'goma is not running.',
            'url': '',
            'pids': 'unknown'
        }

      def CompilerProxyRunning(self):
        return self.running

      def ExecCompilerProxy(self):
        self.running = True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._UpdateHook()
    self.assertFalse(env.running)

  def testUpdateHookRunningNoChange(self):
    class SpyGomaEnv(FakeGomaEnv):
      """Spy GomaEnv for update check"""
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.shutdowned = False
        self.running = True

      def GetCompilerProxyVersion(self):
        return 'GOMA version versionhash@time'

      def ControlCompilerProxy(self, command, check_running=True,
                               need_pids=False):
        if command == '/quitquitquit' and self.running:
          self.shutdowned = True
          self.running = False
          return {'status': False, 'message': 'ok', 'url': '', 'pid': '1234'}
        if command == '/versionz' and self.running:
          return {
              'status': True,
              'message': 'versionhash@time',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        if command == '/progz' and self.running:
          return {
              'status': True,
              'message': '/path/to/compiler_proxy',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        if self.running:
          return {
              'status': True,
              'message': 'ok',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        return {
            'status': False,
            'message': 'goma is not running.',
            'url': '',
            'pids': 'unknown'
        }

      def CompilerProxyRunning(self):
        return self.running

      def ExecCompilerProxy(self):
        self.running = True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._UpdateHook()
    self.assertFalse(env.shutdowned)
    self.assertTrue(env.running)

  def testUpdateHookRunningNoChangeDifferentCaseLetters(self):

    class SpyGomaEnv(FakeGomaEnv):
      """Spy GomaEnv for update check"""

      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.shutdowned = False
        self.running = True

      def CompilerProxyBinary(self):
        return 'c:\\path\\to\\compiler_proxy.exe'

      def GetCompilerProxyVersion(self):
        return 'GOMA version versionhash@time'

      def ControlCompilerProxy(self,
                               command,
                               check_running=True,
                               need_pids=False):
        if command == '/quitquitquit' and self.running:
          self.shutdowned = True
          self.running = False
          return {'status': False, 'message': 'ok', 'url': '', 'pid': '1234'}
        if command == '/versionz' and self.running:
          return {
              'status': True,
              'message': 'versionhash@time',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        if command == '/progz' and self.running:
          return {
              'status': True,
              'message': 'C:\\path\\to\\compiler_proxy.exe',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        if self.running:
          return {
              'status': True,
              'message': 'ok',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        return {
            'status': False,
            'message': 'goma is not running.',
            'url': '',
            'pids': 'unknown'
        }

      def CompilerProxyRunning(self):
        return self.running

      def ExecCompilerProxy(self):
        self.running = True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._UpdateHook()
    self.assertFalse(env.shutdowned)
    self.assertTrue(env.running)

  def testUpdateHookRunningChange(self):
    class SpyGomaEnv(FakeGomaEnv):
      """Spy GomaEnv for update check"""
      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.shutdowned = False
        self.running = True

      def GetCompilerProxyVersion(self):
        return 'GOMA version newversionhash@time'

      def ControlCompilerProxy(self, command, check_running=True,
                               need_pids=False):
        if command == '/quitquitquit' and self.running:
          self.shutdowned = True
          self.running = False
          return {'status': False, 'message': 'ok', 'url': '', 'pid': '1234'}
        if command == '/versionz' and self.running:
          if self.shutdowned:
            return {
                'status': True,
                'message': 'newversionhash@time',
                'url': 'http://127.0.0.1:8088',
                'pid': '1234'
            }
          return {
              'status': True,
              'message': 'versionhash@time',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        if command == '/progz' and self.running:
          return {
              'status': True,
              'message': '/path/to/compiler_proxy',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        if self.running:
          return {
              'status': True,
              'message': 'ok',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        return {
            'status': False,
            'message': 'goma is not running.',
            'url': '',
            'pids': 'unknown'
        }

      def CompilerProxyRunning(self):
        return self.running

      def ExecCompilerProxy(self):
        self.running = True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._UpdateHook()
    self.assertTrue(env.shutdowned)
    self.assertTrue(env.running)

  def testUpdateHookRunningDifferentBinary(self):

    class SpyGomaEnv(FakeGomaEnv):
      """Spy GomaEnv for update check"""

      def __init__(self):
        super(SpyGomaEnv, self).__init__()
        self.shutdowned = False
        self.running = True

      def GetCompilerProxyVersion(self):
        return 'GOMA version newversionhash@time'

      def ControlCompilerProxy(self,
                               command,
                               check_running=True,
                               need_pids=False):
        if command == '/quitquitquit' and self.running:
          self.shutdowned = True
          self.running = False
          return {'status': False, 'message': 'ok', 'url': '', 'pid': '1234'}
        if command == '/versionz' and self.running:
          if self.shutdowned:
            return {
                'status': True,
                'message': 'newversionhash@time',
                'url': 'http://127.0.0.1:8088',
                'pid': '1234'
            }
          return {
              'status': True,
              'message': 'versionhash@time',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        if command == '/progz' and self.running:
          return {
              'status': True,
              'message': '/other/path-to/compiler_proxy',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        if self.running:
          return {
              'status': True,
              'message': 'ok',
              'url': 'http://127.0.0.1:8088',
              'pid': '1234'
          }
        return {
            'status': False,
            'message': 'goma is not running.',
            'url': '',
            'pids': 'unknown'
        }

      def CompilerProxyRunning(self):
        return self.running

      def ExecCompilerProxy(self):
        self.running = True

    env = SpyGomaEnv()
    driver = self._module.GomaDriver(env)
    driver._UpdateHook()
    self.assertFalse(env.shutdowned)
    self.assertTrue(env.running)


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

  option_parser = optparse.OptionParser()
  option_parser.add_option('--goma-dir', default=None,
                           help='absolute or relative to goma top dir')
  option_parser.add_option(
      '--platform',
      help='goma platform type.',
      default={
          'linux2': 'linux',
          'darwin': 'mac',
          'win64': 'win64',
          'cygwin': 'win'
      }.get(sys.platform, None),
      choices=('linux', 'mac', 'goobuntu', 'win64'))
  option_parser.add_option('--goma-service-account-json-file',
                           help='goma service account JSON file')
  option_parser.add_option('--small', action='store_true',
                           help='Check small tests only.')
  option_parser.add_option('--verbosity', default=1,
                           help='Verbosity of tests.')
  option_parser.add_option('--port', default='8200',
                           help='compiler_proxy port for large test')
  options, _ = option_parser.parse_args()

  platform_specific = GetPlatformSpecific(options.platform)

  print('testdir:%s' % test_dir)
  if options.goma_dir:
    goma_ctl_path = os.path.abspath(options.goma_dir)
  else:
    goma_ctl_path = os.path.abspath(
        platform_specific.GetDefaultGomaCtlPath(test_dir))
  del sys.argv[1:]

  # Execute test.
  suite = unittest.TestSuite()
  suite.addTest(
      GetParameterizedTestSuite(GomaCtlSmallTest,
                                goma_ctl_path=goma_ctl_path,
                                platform_specific=platform_specific))
  if not options.small:
    suite.addTest(
        GetParameterizedTestSuite(GomaEnvTest,
                                  goma_ctl_path=goma_ctl_path,
                                  platform_specific=platform_specific))
    oauth2_file = options.goma_service_account_json_file
    if not oauth2_file and platform_specific.GetCred():
      oauth2_file = platform_specific.GetCred()
    assert oauth2_file
    suite.addTest(
        GetParameterizedTestSuite(
            GomaCtlLargeTest,
            goma_ctl_path=goma_ctl_path,
            platform_specific=platform_specific,
            oauth2_file=oauth2_file,
            port=options.port))
  result = unittest.TextTestRunner(verbosity=options.verbosity).run(suite)

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

# TODO: write tests for GomaEnv and GomaBackend.
