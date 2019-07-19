// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/dart_analyzer_flags.h"

#include "base/path.h"
#include "glog/logging.h"
#include "gtest/gtest.h"

namespace devtools_goma {

TEST(DartAnalyzerFlagsTest, Basic) {
  const std::string dart_lib_path = file::JoinPath("..", "other", "lib.dart");
  const std::vector<std::string> args{"dartanalyzer",
                                      "--dart-sdk",
                                      "dart-sdk",
                                      "-DFOO=bar",
                                      "--packages=.packages",
                                      "--x-perf-report",
                                      "perf_report.yaml",
                                      "--url-mapping=myLibrary,lib.dart",
                                      "--url-mapping=otherLib," + dart_lib_path,
                                      "library.dart"};

  const std::string cwd = "";
  DartAnalyzerFlags flags(args, cwd);
  EXPECT_TRUE(flags.is_successful());
  std::vector<std::string> expected_inputs = {"library.dart", ".packages"};
  EXPECT_EQ(expected_inputs, flags.input_filenames());

  EXPECT_EQ(std::vector<std::string>{"perf_report.yaml"}, flags.output_files());
  EXPECT_EQ(".packages", flags.packages_file());
  EXPECT_EQ("dart-sdk", flags.dart_sdk());

  absl::flat_hash_map<std::string, std::string> expected_mappings = {
      {"myLibrary", "lib.dart"}, {"otherLib", dart_lib_path}};
  EXPECT_EQ(expected_mappings, flags.url_mappings());
}

TEST(DartAnalyzerFlagsTest, IsDartAnalyzerCommand) {
  EXPECT_TRUE(DartAnalyzerFlags::IsDartAnalyzerCommand("dartanalyzer"));
  EXPECT_TRUE(
      DartAnalyzerFlags::IsDartAnalyzerCommand("/usr/bin/dartanalyzer"));
  EXPECT_TRUE(DartAnalyzerFlags::IsDartAnalyzerCommand(
      "/usr/local/google/home/goma/dart-sdk/bin/dartanalyzer"));

  EXPECT_TRUE(DartAnalyzerFlags::IsDartAnalyzerCommand("dartanalyzer.exe"));

  EXPECT_FALSE(DartAnalyzerFlags::IsDartAnalyzerCommand("foo"));
  EXPECT_FALSE(DartAnalyzerFlags::IsDartAnalyzerCommand("bar"));
}

TEST(DartAnalyzerFlagsTest, GetCompilerName) {
  EXPECT_EQ("dartanalyzer", DartAnalyzerFlags::GetCompilerName("dartanalyzer"));
  EXPECT_EQ("dartanalyzer",
            DartAnalyzerFlags::GetCompilerName("dartanalyzer.exe"));
}

}  // namespace devtools_goma
