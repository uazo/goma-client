// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_CXX_VC_COMPILER_INFO_BUILDER_H_
#define DEVTOOLS_GOMA_CLIENT_CXX_VC_COMPILER_INFO_BUILDER_H_

#include <string>
#include <vector>

#include "compiler_specific.h"
#include "cxx_compiler_info_builder.h"
#include "vc_flags.h"

MSVC_PUSH_DISABLE_WARNING_FOR_PROTO()
#include "prototmp/compiler_info_data.pb.h"
MSVC_POP_WARNING()

namespace devtools_goma {

class VCCompilerInfoBuilder : public CxxCompilerInfoBuilder {
 public:
  ~VCCompilerInfoBuilder() override = default;

  void SetTypeSpecificCompilerInfo(
      const CompilerFlags& flags,
      const std::string& local_compiler_path,
      const std::string& abs_local_compiler_path,
      const std::vector<std::string>& compiler_info_envs,
      CompilerInfoData* data) const override;

  // Parses output of "cl.exe", and extracts |version| and |target|.
  static bool ParseVCVersion(const std::string& vc_logo,
                             std::string* version,
                             std::string* target);

  // Execute VC and get the string output for VC version
  static bool GetVCVersion(const std::string& cl_exe_path,
                           const std::vector<std::string>& env,
                           const std::string& cwd,
                           std::string* version,
                           std::string* target);

  // Parses output of "cl.exe /nologo /Bxvcflags.exe non-exist-file.cpp" (C++)
  // or "cl.exe /nologo /B1vcflags.exe non-exist-file.c" (C),
  // and extracts |include_paths| and |predefined macros| in
  // "#define FOO X\n" format.
  // |predefined_macros| may be NULL (don't capture predefined macros
  // in this case).
  static bool ParseVCOutputString(const std::string& output,
                                  std::vector<std::string>* include_paths,
                                  std::string* predefined_macros);

  static bool GetVCDefaultValues(
      const std::string& cl_exe_path,
      const std::string& vcflags_path,
      const std::vector<std::string>& compiler_info_flags,
      const std::vector<std::string>& compiler_info_envs,
      const std::string& cwd,
      const std::string& lang,
      CompilerInfoData* compiler_info);

 private:
  // SetTypeSpecificCompilerInfo for cl.exe
  void SetClexeSpecificCompilerInfo(
      const VCFlags& flags,
      const std::string& local_compiler_path,
      const std::string& abs_local_compiler_path,
      const std::vector<std::string>& compiler_info_envs,
      CompilerInfoData* data) const;
  // SetTypeSpecificCompilerInfo for clang-cl.exe
  void SetClangClSpecificCompilerInfo(
      const VCFlags& flags,
      const std::string& local_compiler_path,
      const std::string& abs_local_compiler_path,
      const std::vector<std::string>& compiler_info_envs,
      CompilerInfoData* data) const;
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_CXX_VC_COMPILER_INFO_BUILDER_H_
