// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rust/rustc_compiler_type_specific.h"

#include "glog/logging.h"
#include "glog/stl_logging.h"
#include "rust/rustc_include_processor.h"

namespace devtools_goma {

std::unique_ptr<CompilerInfoData>
RustcCompilerTypeSpecific::BuildCompilerInfoData(
    const CompilerFlags& flags,
    const std::string& local_compiler_path,
    const std::vector<std::string>& compiler_info_envs) {
  return compiler_info_builder_.FillFromCompilerOutputs(
      flags, local_compiler_path, compiler_info_envs);
}

// Runs include processor.
// |trace_id| is passed from compile_task for logging purpose.
CompilerTypeSpecific::IncludeProcessorResult
RustcCompilerTypeSpecific::RunIncludeProcessor(
    const std::string& trace_id,
    const CompilerFlags& compiler_flags,
    const CompilerInfo& compiler_info,
    const CommandSpec& command_spec,
    FileStatCache* file_stat_cache) {
  const RustcFlags& rustc_flags =
      static_cast<const RustcFlags&>(compiler_flags);
  const RustcCompilerInfo& rustc_compiler_info =
      ToRustcCompilerInfo(compiler_info);

  RustcIncludeProcessor include_processor;
  std::set<std::string> required_files;
  std::string error_reason;
  if (!include_processor.Run(rustc_flags, rustc_compiler_info, &required_files,
                             &error_reason)) {
    return IncludeProcessorResult::ErrorToLog(error_reason);
  }

  LOG(INFO) << "rustc required_files: " << required_files;
  return IncludeProcessorResult::Ok(std::move(required_files));
}

}  // namespace devtools_goma
