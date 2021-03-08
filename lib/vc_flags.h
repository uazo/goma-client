// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_LIB_VC_FLAGS_H_
#define DEVTOOLS_GOMA_LIB_VC_FLAGS_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "lib/cxx_flags.h"
#include "lib/flag_parser.h"

namespace devtools_goma {

class VCFlags : public CxxFlags {
 public:
  VCFlags(const std::vector<std::string>& args, const std::string& cwd);

  const std::vector<std::string>& include_dirs() const { return include_dirs_; }
  const std::vector<std::string>& root_includes() const {
    return root_includes_;
  }
  const std::vector<std::pair<std::string, bool>>& commandline_macros() const {
    return commandline_macros_;
  }
  const std::string& thinlto_index() const { return thinlto_index_; }
  const std::string& fdebug_compilation_dir() const {
    return fdebug_compilation_dir_;
  }
  const std::string& fcoverage_compilation_dir() const {
    return fcoverage_compilation_dir_;
  }
  const std::string& ffile_compilation_dir() const {
    return ffile_compilation_dir_;
  }

  bool is_cplusplus() const override { return is_cplusplus_; }
  bool ignore_stdinc() const { return ignore_stdinc_; }
  bool require_mspdbserv() const { return require_mspdbserv_; }
  bool has_Brepro() const { return has_Brepro_; }
  bool has_fcoverage_mapping() const { return has_fcoverage_mapping_; }

  std::string compiler_name() const override;

  CompilerFlagType type() const override { return CompilerFlagType::Clexe; }

  bool IsClientImportantEnv(const char* env) const override;
  bool IsServerImportantEnv(const char* env) const override;

  static void DefineFlags(FlagParser* parser);
  static bool ExpandArgs(const std::string& cwd,
                         const std::vector<std::string>& args,
                         std::vector<std::string>* expanded_args,
                         std::vector<std::string>* optional_input_filenames);

  const std::string& creating_pch() const { return creating_pch_; }
  const std::string& using_pch() const { return using_pch_; }
  const std::string& using_pch_filename() const { return using_pch_filename_; }
  const std::string& resource_dir() const { return resource_dir_; }

  const std::string& implicit_macros() const { return implicit_macros_; }

  // TODO: check flags_->is_linking() etc.

  static bool IsClangClCommand(absl::string_view arg);
  static bool IsVCCommand(absl::string_view arg);
  static std::string GetCompilerName(absl::string_view arg);

 private:
  friend class VCFlagsTest;
  // Compose output file path
  static std::string ComposeOutputFilePath(
      const std::string& input_filename,
      const std::string& output_file_or_dir,
      const std::string& output_file_ext);

  std::vector<std::string> include_dirs_;
  std::vector<std::string> root_includes_;
  // The second value is true if the macro is defined and false if undefined.
  std::vector<std::pair<std::string, bool>> commandline_macros_;
  bool is_cplusplus_;
  bool ignore_stdinc_;
  bool has_Brepro_;
  std::string creating_pch_;
  std::string using_pch_;
  // The filename of .pch, if specified.
  std::string using_pch_filename_;
  bool require_mspdbserv_;
  std::string resource_dir_;
  std::string implicit_macros_;
  bool has_fcoverage_mapping_ = false;
  bool has_ftime_trace_;
  std::string thinlto_index_;
  std::string fdebug_compilation_dir_;
  std::string fcoverage_compilation_dir_;
  std::string ffile_compilation_dir_;
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_LIB_VC_FLAGS_H_
