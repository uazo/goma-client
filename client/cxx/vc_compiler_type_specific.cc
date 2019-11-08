// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vc_compiler_type_specific.h"
#include "glog/logging.h"
#include "linker/linker_input_processor/thinlto_import_processor.h"

namespace devtools_goma {

bool VCCompilerTypeSpecific::RemoteCompileSupported(const std::string& trace_id,
                                                    const CompilerFlags& flags,
                                                    bool verify_output) const {
  const VCFlags& vc_flag = static_cast<const VCFlags&>(flags);
  // GOMA doesn't work with PCH so we generate it only for local builds.
  if (!vc_flag.creating_pch().empty()) {
    LOG(INFO) << trace_id
              << " force fallback. cannot create pch in goma backend.";
    return false;
  }
  if (vc_flag.require_mspdbserv()) {
    LOG(INFO) << trace_id
              << " force fallback. cannot run mspdbserv in goma backend.";
    return false;
  }

  return true;
}

std::unique_ptr<CompilerInfoData> VCCompilerTypeSpecific::BuildCompilerInfoData(
    const CompilerFlags& flags,
    const std::string& local_compiler_path,
    const std::vector<std::string>& compiler_info_envs) {
  return compiler_info_builder_.FillFromCompilerOutputs(
      flags, local_compiler_path, compiler_info_envs);
}

CompilerTypeSpecific::IncludeProcessorResult
VCCompilerTypeSpecific::RunIncludeProcessor(const std::string& trace_id,
                                            const CompilerFlags& compiler_flags,
                                            const CompilerInfo& compiler_info,
                                            const CommandSpec& command_spec,
                                            FileStatCache* file_stat_cache) {
  DCHECK_EQ(CompilerFlagType::Clexe, compiler_flags.type());

  const VCFlags& flags = static_cast<const VCFlags&>(compiler_flags);

  if (!flags.thinlto_index().empty()) {
    ThinLTOImportProcessor processor;
    std::set<std::string> required_files;
    if (!processor.GetIncludeFiles(flags.thinlto_index(), flags.cwd(),
                                   &required_files)) {
      LOG(ERROR) << trace_id << " failed to get ThinLTO imports";
      return IncludeProcessorResult::ErrorToLog(
          "failed to get ThinLTO imports");
    }
    return IncludeProcessorResult::Ok(std::move(required_files));
  }

  // Invoke base class method.
  return CxxCompilerTypeSpecific::RunIncludeProcessor(
      trace_id, compiler_flags, compiler_info, command_spec, file_stat_cache);
}

}  // namespace devtools_goma
