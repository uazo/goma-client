# Copyright (c) 2019 The Goma Authors. All rights reserved.


def CheckChangeOnUpload(input_api, output_api):
  return input_api.canned_checks.CheckChangedLUCIConfigs(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return input_api.canned_checks.CheckChangedLUCIConfigs(input_api, output_api)
