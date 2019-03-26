// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_CXX_CXX_COMPILER_INFO_BUILDER_H_
#define DEVTOOLS_GOMA_CLIENT_CXX_CXX_COMPILER_INFO_BUILDER_H_

#include <string>
#include <vector>

#include "compiler_info_builder.h"

namespace devtools_goma {

// CxxCompilerInfoBuilder is a base class of CompilerInfoBuilder for C/C++
// language. This contains several methods to calculate C/C++ CompilerInfo,
// e.g. gcc, clang, g++, clang++, cl.exe, clang-cl, nacl-gcc, pnacl-clang
class CxxCompilerInfoBuilder : public CompilerInfoBuilder {
 public:
  // Parse |gcc_output| to get list of subprograms.
  static void ParseGetSubprogramsOutput(const std::string& gcc_output,
                                        std::vector<std::string>* paths);

  // Returns true on success, and |subprograms| will have full path of
  // external subprograms or empty vector if not found.
  // Returns false on failure.
  static bool GetSubprograms(
      const std::string& gcc_path,
      const std::string& lang,
      const std::vector<std::string>& compiler_info_flags,
      const std::vector<std::string>& compiler_info_envs,
      const std::string& cwd,
      bool warn_on_empty,
      std::vector<std::string>* subprograms);

  // Get real subprogram path.
  // See: go/ma/resources-for-developers/goma-compiler-selection-mechanism
  static std::string GetRealSubprogramPath(const std::string& subprogram_path);

  static bool SubprogramInfoFromPath(const std::string& user_specified_path,
                                     const std::string& abs_path,
                                     CompilerInfoData::SubprogramInfo* s);

  void SetLanguageExtension(CompilerInfoData* data) const override;
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_CXX_CXX_COMPILER_INFO_BUILDER_H_
