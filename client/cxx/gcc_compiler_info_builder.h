// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_CXX_GCC_COMPILER_INFO_BUILDER_H_
#define DEVTOOLS_GOMA_CLIENT_CXX_GCC_COMPILER_INFO_BUILDER_H_

#include <map>
#include <ostream>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "autolock_timer.h"
#include "cxx_compiler_info_builder.h"
#include "gcc_flags.h"

namespace devtools_goma {

// GCCCompilerInfoBuilder is a compiler info builder for gcc-like compilers
// e.g. gcc, g++, clang, clang++, pnacl-clang, etc.
// See VCCompilerInfoBuilder for cl.exe and clang-cl.exe.
class GCCCompilerInfoBuilder : public CxxCompilerInfoBuilder {
 public:
  ~GCCCompilerInfoBuilder() override = default;

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

  std::string GetCompilerName(const CompilerInfoData& data) const override;

  // Returns false if GetExtraSubprograms failed to get subprogram
  // info while a subprogram exists.
  static bool GetExtraSubprograms(
      const std::string& normal_gcc_path,
      const GCCFlags& flags,
      const std::vector<std::string>& compiler_info_envs,
      CompilerInfoData* compiler_info);

  // Parses compile flags for subprograms, especially clang plugins.
  static void ParseSubprogramFlags(const std::string& normal_gcc_path,
                                   const GCCFlags& flags,
                                   std::vector<std::string>* clang_plugins,
                                   std::vector<std::string>* B_options,
                                   bool* no_integrated_as);

  // Returns true if |subprogram_paths| contain a path for as (assembler).
  static bool HasAsPath(const std::vector<std::string>& subprogram_paths);

  // Get real compiler path.
  // See: go/ma/resources-for-developers/goma-compiler-selection-mechanism
  static std::string GetRealCompilerPath(const std::string& normal_gcc_path,
                                         const std::string& cwd,
                                         const std::vector<std::string>& envs);
};

};  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_CXX_GCC_COMPILER_INFO_BUILDER_H_
