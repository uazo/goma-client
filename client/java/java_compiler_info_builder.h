// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_JAVA_JAVA_COMPILER_INFO_BUILDER_H_
#define DEVTOOLS_GOMA_CLIENT_JAVA_JAVA_COMPILER_INFO_BUILDER_H_

#include <string>
#include <vector>

#include "compiler_info_builder.h"
#include "gtest/gtest_prod.h"

namespace devtools_goma {

class JavacCompilerInfoBuilder : public CompilerInfoBuilder {
 public:
  ~JavacCompilerInfoBuilder() override = default;

  void SetLanguageExtension(CompilerInfoData* data) const override;

  void SetTypeSpecificCompilerInfo(
      const CompilerFlags& flags,
      const std::string& local_compiler_path,
      const std::string& abs_local_compiler_path,
      const std::vector<std::string>& compiler_info_envs,
      CompilerInfoData* data) const override;

  // Parses output of "javac", and extracts |version|.
  static bool ParseJavacVersion(const std::string& vc_logo,
                                std::string* version);

  // Execute javac and get the string output for javac version
  static bool GetJavacVersion(
      const std::string& javac,
      const std::vector<std::string>& compiler_info_envs,
      const std::string& cwd,
      std::string* version);
};

class JavaCompilerInfoBuilder : public CompilerInfoBuilder {
 public:
  ~JavaCompilerInfoBuilder() override = default;

  void SetLanguageExtension(CompilerInfoData* data) const override;

  void SetTypeSpecificCompilerInfo(
      const CompilerFlags& flags,
      const std::string& local_compiler_path,
      const std::string& abs_local_compiler_path,
      const std::vector<std::string>& compiler_info_envs,
      CompilerInfoData* data) const override;
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_JAVA_JAVA_COMPILER_INFO_BUILDER_H_
