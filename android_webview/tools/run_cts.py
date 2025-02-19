#!/usr/bin/env python
#
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the CTS test APKs stored in CIPD."""

import argparse
import json
import logging
import os
import shutil
import sys
import tempfile
import zipfile


sys.path.append(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, 'build', 'android'))
import devil_chromium  # pylint: disable=import-error, unused-import
from devil.android.ndk import abis  # pylint: disable=import-error
from devil.android.sdk import version_codes  # pylint: disable=import-error
from devil.android.tools import script_common  # pylint: disable=import-error
from devil.utils import cmd_helper  # pylint: disable=import-error
from pylib.utils import test_filter # pylint: disable=import-error

# cts test archives for all platforms are stored in this bucket
# contents need to be updated if there is an important fix to any of
# the tests

_TEST_RUNNER_PATH = os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir,
    'build', 'android', 'test_runner.py')

_EXPECTED_FAILURES_FILE = os.path.join(
    os.path.dirname(__file__), 'cts_config', 'expected_failure_on_bot.json')
_WEBVIEW_CTS_GCS_PATH_FILE = os.path.join(
    os.path.dirname(__file__), 'cts_config', 'webview_cts_gcs_path.json')
_CTS_ARCHIVE_DIR = os.path.join(os.path.dirname(__file__), 'cts_archive')

_SDK_PLATFORM_DICT = {
    version_codes.LOLLIPOP: 'L',
    version_codes.LOLLIPOP_MR1: 'L',
    version_codes.MARSHMALLOW: 'M',
    version_codes.NOUGAT: 'N',
    version_codes.NOUGAT_MR1: 'N',
    version_codes.OREO: 'O',
    version_codes.OREO_MR1: 'O'
}

# TODO(aluo): support 'x86' and 'x86_64'
_SUPPORTED_ARCH_DICT = {
    abis.ARM_64: 'arm64',
    # The test apks under 'arm64' support both arm and arm64 devices.
    abis.ARM: 'arm64',
}

FILE_FILTER_OPT = '--test-launcher-filter-file'
TEST_FILTER_OPT = '--test-filter'
ISOLATED_FILTER_OPT = '--isolated-script-test-filter'

def GetCtsInfo(arch, platform, item):
  """Gets contents of CTS Info for arch and platform.

  See _WEBVIEW_CTS_GCS_PATH_FILE
  """
  with open(_WEBVIEW_CTS_GCS_PATH_FILE) as f:
    cts_gcs_path_info = json.load(f)
  try:
    return cts_gcs_path_info[arch][platform][item]
  except KeyError:
    raise Exception('No %s info available for arch:%s, android:%s' %
                    (item, arch, platform))


def GetCTSModuleNames(arch, platform):
  """Gets the module apk name of the arch and platform"""
  test_runs = GetCtsInfo(arch, platform, 'test_runs')
  return [os.path.basename(r['apk']) for r in test_runs]


def GetExpectedFailures():
  """Gets list of tests expected to fail in <class>#<method> format.

  See _EXPECTED_FAILURES_FILE
  """
  with open(_EXPECTED_FAILURES_FILE) as f:
    expected_failures_info = json.load(f)
  expected_failures = []
  for class_name, methods in expected_failures_info.iteritems():
    expected_failures.extend(['%s#%s' % (class_name, m['name'])
                              for m in methods])
  return expected_failures


def GetTestRunFilterArg(args, test_run):
  """ Filters specified in args override others """
  filter_args = []

  if args.test_filter_file:
    filter_args.append(FILE_FILTER_OPT + '=' + args.test_filter_file)
  if args.test_filter:
    filter_args.append(TEST_FILTER_OPT + '=' + args.test_filter)
  if args.isolated_script_test_filter:
    filter_args.append(ISOLATED_FILTER_OPT + '='
                       + args.isolated_script_test_filter)
  if filter_args:
    return filter_args

  skips = []
  if args.skip_expected_failures:
    skips = GetExpectedFailures()

  excludes = test_run.get("excludes", [])
  includes = test_run.get("includes", [])
  assert len(excludes) == 0 or len(includes) == 0, \
         "test_runs error, can't have both includes and excludes: %s" % test_run
  if len(includes) > 0:
    return ['--test-filter=' + ':'.join([i["match"] for i in includes])]
  else:
    skips.extend([i["match"] for i in excludes])
    if len(skips) > 0:
      return ['--test-filter=' + "-" + ':'.join(skips)]
    return []


def RunCTS(test_runner_args, local_cts_dir, apk, json_results_file=None):
  """Run tests in apk using test_runner script at _TEST_RUNNER_PATH.

  Returns the script result code, test results will be stored in
  the json_results_file file if specified.
  """

  local_test_runner_args = test_runner_args + ['--test-apk',
                                               os.path.join(local_cts_dir, apk)]

  if json_results_file:
    local_test_runner_args += ['--json-results-file=%s' %
                               json_results_file]
  return cmd_helper.RunCmd(
      [_TEST_RUNNER_PATH, 'instrumentation'] + local_test_runner_args)


def MergeTestResults(existing_results_json, additional_results_json):
  """Appends results in additional_results_json to existing_results_json."""
  for k, v in additional_results_json.iteritems():
    if k not in existing_results_json:
      existing_results_json[k] = v
    else:
      if isinstance(v, dict):
        if not isinstance(existing_results_json[k], dict):
          raise NotImplementedError(
              "Can't merge results field %s of different types" % v)
        existing_results_json[k].update(v)
      elif isinstance(v, list):
        if not isinstance(existing_results_json[k], list):
          raise NotImplementedError(
              "Can't merge results field %s of different types" % v)
        existing_results_json[k].extend(v)
      else:
        raise NotImplementedError(
            "Can't merge results field %s that is not a list or dict" % v)


def ExtractCTSZip(args, arch):
  """Extract the CTS tests for args.platform.

  Extract the CTS zip file from _CTS_ARCHIVE_DIR to
  apk_dir if specified, or a new temporary directory if not.
  Returns following tuple (local_cts_dir, base_cts_dir, delete_cts_dir):
    local_cts_dir - CTS extraction location for current arch and platform
    base_cts_dir - Root directory for all the arches and platforms
    delete_cts_dir - Set if the base_cts_dir was created as a temporary
    directory
  """
  base_cts_dir = None
  delete_cts_dir = False
  relative_cts_zip_path = GetCtsInfo(arch, args.platform, 'filename')

  if args.apk_dir:
    base_cts_dir = args.apk_dir
  else:
    base_cts_dir = tempfile.mkdtemp()
    delete_cts_dir = True

  cts_zip_path = os.path.join(_CTS_ARCHIVE_DIR, relative_cts_zip_path)
  local_cts_dir = os.path.join(base_cts_dir,
                               GetCtsInfo(arch, args.platform,
                                          'unzip_dir')
                              )
  zf = zipfile.ZipFile(cts_zip_path, 'r')
  zf.extractall(local_cts_dir)
  return (local_cts_dir, base_cts_dir, delete_cts_dir)


def RunAllCTSTests(args, arch, test_runner_args):
  """Run CTS tests downloaded from _CTS_BUCKET.

  Downloads CTS tests from bucket, runs them for the
  specified platform+arch, then creates a single
  results json file (if specified)
  Returns 0 if all tests passed, otherwise
  returns the failure code of the last failing
  test.
  """
  local_cts_dir, base_cts_dir, delete_cts_dir = ExtractCTSZip(args, arch)
  cts_result = 0
  json_results_file = args.json_results_file
  try:
    cts_test_runs = GetCtsInfo(arch, args.platform, 'test_runs')
    cts_results_json = {}
    for cts_test_run in cts_test_runs:
      iteration_cts_result = 0

      test_apk = cts_test_run['apk']
      # If --module-apk is specified then skip tests in all other modules
      if args.module_apk and os.path.basename(test_apk) != args.module_apk:
        continue

      iter_test_runner_args = test_runner_args + GetTestRunFilterArg(
          args, cts_test_run)

      if json_results_file:
        with tempfile.NamedTemporaryFile() as iteration_json_file:
          iteration_cts_result = RunCTS(iter_test_runner_args, local_cts_dir,
                                        test_apk, iteration_json_file.name)
          with open(iteration_json_file.name) as f:
            additional_results_json = json.load(f)
            MergeTestResults(cts_results_json, additional_results_json)
      else:
        iteration_cts_result = RunCTS(iter_test_runner_args, local_cts_dir,
                                      test_apk)
      if iteration_cts_result:
        cts_result = iteration_cts_result
    if json_results_file:
      with open(json_results_file, 'w') as f:
        json.dump(cts_results_json, f, indent=2)
  finally:
    if delete_cts_dir and base_cts_dir:
      shutil.rmtree(base_cts_dir)

  return cts_result


def DeterminePlatform(device):
  """Determines the platform based on the Android SDK level

  Returns the first letter of the platform in uppercase
  if platform is found, otherwise returns None
  """
  return _SDK_PLATFORM_DICT.get(device.build_version_sdk)


def DetermineArch(device):
  """Determines which architecture to use based on the device properties

  Args:
    device: The DeviceUtils instance
  Returns:
    The formatted arch string (as expected by CIPD)
  Raises:
    Exception: if device architecture is not currently supported by this script.
  """
  arch = _SUPPORTED_ARCH_DICT.get(device.product_cpu_abi)
  if not arch:
    raise Exception('Could not find CIPD bucket for your device arch (' +
                    device.product_cpu_abi +
                    '), please specify with --arch')
  logging.info('Guessing arch=%s because product.cpu.abi=%s', arch,
               device.product_cpu_abi)
  return arch


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--arch',
      choices=list(set(_SUPPORTED_ARCH_DICT.values())),
      default=None,
      type=str,
      help=('Architecture to for CTS tests. Will auto-determine based on '
            'the device ro.product.cpu.abi property.'))
  parser.add_argument(
      '--platform',
      choices=['L', 'M', 'N', 'O'],
      required=False,
      default=None,
      help='Android platform version for CTS tests. '
           'Will auto-determine based on SDK level by default.')
  parser.add_argument(
      '--skip-expected-failures',
      action='store_true',
      help="Option to skip all tests that are expected to fail.  Can't be used "
           "with test filters.")
  parser.add_argument(
      '--apk-dir',
      help='Directory to extract CTS APKs to. '
           'Will use temp directory by default.')
  parser.add_argument(
      '--test-launcher-summary-output',
      '--json-results-file',
      '--write-full-results-to',
      '--isolated-script-test-output',
      dest='json_results_file', type=os.path.realpath,
      help='If set, will dump results in JSON form to the specified file. '
           'Note that this will also trigger saving per-test logcats to '
           'logdog.')
  parser.add_argument(
      '-m',
      '--module-apk',
      dest='module_apk',
      help='CTS module apk name in ' + _WEBVIEW_CTS_GCS_PATH_FILE +
      ' file, without the path prefix.')

  test_filter.AddFilterOptions(parser)
  script_common.AddDeviceArguments(parser)

  args, test_runner_args = parser.parse_known_args()
  devil_chromium.Initialize()

  devices = script_common.GetDevices(args.devices, args.blacklist_file)
  device = devices[0]
  if len(devices) > 1:
    logging.warning('Only single device supported, using 1st of %d devices: %s',
                    len(devices), device.serial)
  test_runner_args.extend(['-d', device.serial])

  if args.platform is None:
    args.platform = DeterminePlatform(device)
    if args.platform is None:
      raise Exception('Could not auto-determine device platform, '
                      'please specifiy --platform')

  arch = args.arch if args.arch else DetermineArch(device)

  if (args.test_filter_file or args.test_filter
      or args.isolated_script_test_filter):
    if args.skip_expected_failures:
      # TODO(aluo): allow both options to be used together so that expected
      # failures in the filtered test set can be skipped
      raise Exception('--skip-expected-failures and test filters are mutually'
                      ' exclusive')
    # TODO(aluo): auto-determine the module based on the test filter and the
    # available tests in each module
    if not args.module_apk:
      args.module_apk = 'CtsWebkitTestCases.apk'

  platform_modules = GetCTSModuleNames(arch, args.platform)
  if args.module_apk and args.module_apk not in platform_modules:
    raise Exception('--module-apk for arch==' + arch + 'and platform=='
                    + args.platform + ' must be one of: '
                    + ', '.join(platform_modules))

  return RunAllCTSTests(args, arch, test_runner_args)


if __name__ == '__main__':
  sys.exit(main())
