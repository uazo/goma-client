// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_DART_ANALYZER_DART_ANALYZER_COMPILER_INFO_BUILDER_H_
#define DEVTOOLS_GOMA_CLIENT_DART_ANALYZER_DART_ANALYZER_COMPILER_INFO_BUILDER_H_

#include <string>
#include <vector>

#include "compiler_info_builder.h"
#include "gtest/gtest_prod.h"

namespace devtools_goma {

class DartAnalyzerCompilerInfoBuilder : public CompilerInfoBuilder {
 public:
  ~DartAnalyzerCompilerInfoBuilder() override = default;

  void SetLanguageExtension(CompilerInfoData* data) const override;

  void SetTypeSpecificCompilerInfo(
      const CompilerFlags& flags,
      const std::string& local_compiler_path,
      const std::string& abs_local_compiler_path,
      const std::vector<std::string>& compiler_info_envs,
      CompilerInfoData* data) const override;

  void SetCompilerPath(const CompilerFlags& flags,
                       const std::string& local_compiler_path,
                       const std::vector<std::string>& compiler_info_envs,
                       CompilerInfoData* data) const override;

 private:
  // Gets dartanalyzer's version and host.
  bool GetDartAnalyzerVersionHost(
      const std::string& dart_analyzer_path,
      const std::vector<std::string>& compiler_info_envs,
      const std::string& cwd,
      std::string* version,
      std::string* host) const;
  // Parses dartanalyzer's version and host from compiler_output.
  bool ParseDartAnalyzerVersionHost(const std::string& compiler_output,
                                    std::string* version,
                                    std::string* host) const;

  static bool CollectDartAnalyzerResources(
      absl::string_view real_compiler_path,
      std::vector<std::string>* resource_paths);

  FRIEND_TEST(DartAnalyzerCompilerInfoBuilderTest,
              ParseDartAnalyzerVersionTarget);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_DART_ANALYZER_DART_ANALYZER_COMPILER_INFO_BUILDER_H_
